#include <gtest/gtest.h>

#include <algorithm>

#include "app/context.hpp"
#include "disruption/closures.hpp"
#include "search/search.hpp"

using namespace safar;
using disruption::Closure;
using disruption::ClosureError;
using graph::EdgeKind;
using journey::Endpoint;

namespace {

const AppContext& ctx() {
    static const AppContext c = [] {
        Config cfg;
        cfg.data_dir = SAFAR_TEST_DATA_DIR;
        cfg.transport_file = SAFAR_TEST_CONFIG_DIR "/transport.json";
        return AppContext::load(cfg);
    }();
    return c;
}

const graph::Graph& g() { return *ctx().transit; }
const data::Dataset& ds() { return *ctx().dataset; }

std::size_t station_index(const std::string& id) {
    const auto* s = ds().station(id);
    EXPECT_NE(s, nullptr) << id;
    return static_cast<std::size_t>(s - ds().stations().data());
}

Endpoint station(const std::string& id) { return {Endpoint::Type::Station, station_index(id)}; }

Endpoint place(const std::string& id) {
    const auto* p = ds().place(id);
    EXPECT_NE(p, nullptr) << id;
    return {Endpoint::Type::Place, static_cast<std::size_t>(p - ds().places().data())};
}

Closure yellow_cut() { return disruption::segment_closure(ds(), g(), "yellow", "hauz-khas", "green-park"); }

std::size_t count(const std::vector<char>& mask) { return static_cast<std::size_t>(std::count(mask.begin(), mask.end(), 1)); }

// Does any metro leg of the route ride from platform a to platform b (either way)?
bool rides_between(const journey::JourneyResult& r, const journey::Route& route, const std::string& a,
                   const std::string& b) {
    for (const auto& leg : route.itinerary.legs) {
        for (std::size_t k = 0; k + 1 < leg.nodes.size(); ++k) {
            const auto& x = r.graph.node(leg.nodes[k]).id;
            const auto& y = r.graph.node(leg.nodes[k + 1]).id;
            if ((x == a && y == b) || (x == b && y == a)) return true;
        }
    }
    return false;
}

}  // namespace

TEST(Disruptions, StationClosureStopsTrainsStoppingButNotPassing) {
    const auto mask = disruption::blocked_edges(g(), {disruption::station_closure(ds(), "rajiv-chowk")});
    const auto si = station_index("rajiv-chowk");
    std::size_t blocked = 0, rides_through = 0;
    for (graph::EdgeId e = 0; e < g().edge_count(); ++e) {
        const auto& edge = g().edge(e);
        const bool here = g().node(edge.from).station == si || g().node(edge.to).station == si;
        if (edge.kind == EdgeKind::Board || edge.kind == EdgeKind::Alight || edge.kind == EdgeKind::Transfer) {
            EXPECT_EQ(mask[e] != 0, here) << g().node(edge.from).id << " -> " << g().node(edge.to).id;
            blocked += mask[e];
        } else {
            EXPECT_EQ(mask[e], 0) << "only boarding, alighting and changing are blocked";
            rides_through += here && edge.kind == EdgeKind::Ride;
        }
    }
    // Rajiv Chowk: Blue + Yellow, so 2 boards, 2 alights and at least 2 transfers.
    EXPECT_GE(blocked, 6u);
    EXPECT_GE(rides_through, 8u);
    EXPECT_EQ(count(mask), blocked);
}

TEST(Disruptions, SegmentClosureCutsBothDirections) {
    const auto c = yellow_cut();
    EXPECT_EQ(c.a, "green-park");  // ends sorted, so both directions name the same closure
    EXPECT_EQ(c.b, "hauz-khas");
    EXPECT_EQ(c.id(), "segment:yellow:green-park:hauz-khas");
    const auto mask = disruption::blocked_edges(g(), {c});
    ASSERT_EQ(count(mask), 2u);
    for (graph::EdgeId e = 0; e < g().edge_count(); ++e) {
        if (!mask[e]) continue;
        EXPECT_EQ(g().edge(e).kind, EdgeKind::Ride);
        const auto& from = g().node(g().edge(e).from).id;
        EXPECT_TRUE(from == "green-park@yellow" || from == "hauz-khas@yellow") << from;
    }
}

TEST(Disruptions, ValidatesClosures) {
    auto code = [](auto fn) {
        try {
            fn();
        } catch (const ClosureError& e) {
            return e.code;
        }
        return std::string("none");
    };
    EXPECT_EQ(code([] { disruption::station_closure(ds(), "atlantis"); }), "station_not_found");
    EXPECT_EQ(code([] { disruption::segment_closure(ds(), g(), "teal", "green-park", "hauz-khas"); }), "line_not_found");
    EXPECT_EQ(code([] { disruption::segment_closure(ds(), g(), "yellow", "rajiv-chowk", "hauz-khas"); }),
              "invalid_segment");
    EXPECT_EQ(code([] { disruption::segment_closure(ds(), g(), "blue", "green-park", "hauz-khas"); }),
              "invalid_segment");
    EXPECT_EQ(code([] { disruption::segment_closure(ds(), g(), "yellow", "hauz-khas", "hauz-khas"); }),
              "invalid_segment");
}

TEST(Disruptions, StoreAddsRemovesAndVersions) {
    disruption::Store store(ctx().dataset, ctx().transit);
    EXPECT_TRUE(store.snapshot()->empty());
    const auto v0 = store.snapshot()->version;

    EXPECT_TRUE(store.add(yellow_cut()));
    EXPECT_FALSE(store.add(disruption::segment_closure(ds(), g(), "yellow", "green-park", "hauz-khas")));
    EXPECT_TRUE(store.add(disruption::station_closure(ds(), "rajiv-chowk")));
    auto snap = store.snapshot();
    EXPECT_EQ(snap->closures.size(), 2u);
    EXPECT_EQ(snap->version, v0 + 2);
    EXPECT_TRUE(snap->station_closed(station_index("rajiv-chowk")));
    EXPECT_FALSE(snap->station_closed(station_index("hauz-khas")));

    EXPECT_FALSE(store.remove("station:atlantis"));
    EXPECT_TRUE(store.remove("station:rajiv-chowk"));
    EXPECT_EQ(store.snapshot()->closures.size(), 1u);
    EXPECT_EQ(snap->closures.size(), 2u) << "an old snapshot never changes under a reader";

    store.clear();
    EXPECT_TRUE(store.snapshot()->empty());
    EXPECT_EQ(count(store.snapshot()->blocked_edges), 0u);
}

TEST(Disruptions, StoreIsCapped) {
    disruption::Store store(ctx().dataset, ctx().transit);
    for (std::size_t i = 0; i < disruption::Store::kMaxClosures; ++i) {
        ASSERT_TRUE(store.add(disruption::station_closure(ds(), ds().stations()[i].id)));
    }
    EXPECT_THROW(store.add(disruption::station_closure(ds(), ds().stations().back().id)), ClosureError);
}

TEST(Disruptions, SearchRoutesAroundACutLine) {
    const auto from = *g().find("rajiv-chowk");
    const auto to = *g().find("hauz-khas");
    const auto open = search::uniform_cost(g(), from, to);
    ASSERT_TRUE(open.found);

    const auto mask = disruption::blocked_edges(g(), {yellow_cut()});
    search::SearchOptions opt;
    opt.blocked_edges = &mask;
    for (auto algo : search::kAllAlgorithms) {
        const auto r = search::run(algo, g(), from, to, opt);
        ASSERT_TRUE(r.found) << search::to_string(algo);
        for (auto e : r.path) EXPECT_EQ(mask[e], 0) << search::to_string(algo) << " used a closed segment";
        EXPECT_GT(r.cost, open.cost) << search::to_string(algo);
    }
}

TEST(Disruptions, PlannerAvoidsClosuresAndStillFindsARoute) {
    journey::JourneyRequest req;
    req.from = station("rajiv-chowk");
    req.to = station("hauz-khas");
    const auto open = ctx().planner->plan(req);
    ASSERT_FALSE(open.routes.empty());
    EXPECT_TRUE(std::any_of(open.routes.begin(), open.routes.end(), [&](const journey::Route& r) {
        return rides_between(open, r, "green-park@yellow", "hauz-khas@yellow");
    })) << "without closures the Yellow Line is the obvious route";

    disruption::Store store(ctx().dataset, ctx().transit);
    store.add(yellow_cut());
    const auto snap = store.snapshot();
    req.closures = snap.get();
    const auto cut = ctx().planner->plan(req);
    ASSERT_FALSE(cut.routes.empty());
    for (const auto& r : cut.routes) {
        EXPECT_FALSE(rides_between(cut, r, "green-park@yellow", "hauz-khas@yellow")) << r.found_by;
    }
}

TEST(Disruptions, PlannerNeverBoardsOrAlightsAtAClosedStation) {
    disruption::Store store(ctx().dataset, ctx().transit);
    store.add(disruption::station_closure(ds(), "hauz-khas"));
    const auto snap = store.snapshot();
    journey::JourneyRequest req;
    req.from = place("bennett-university");
    req.to = place("hauz-khas-village");
    req.closures = snap.get();
    const auto r = ctx().planner->plan(req);
    ASSERT_FALSE(r.routes.empty());
    const auto closed = station_index("hauz-khas");
    for (const auto& route : r.routes) {
        for (const auto& leg : route.itinerary.legs) {
            if (leg.kind != search::Leg::Kind::Metro && leg.kind != search::Leg::Kind::Transfer) continue;
            EXPECT_NE(r.graph.node(leg.from).station, closed) << route.found_by;
            EXPECT_NE(r.graph.node(leg.to).station, closed) << route.found_by;
        }
    }
}
