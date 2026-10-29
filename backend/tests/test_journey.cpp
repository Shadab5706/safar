#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "app/context.hpp"
#include "search/yen.hpp"

using namespace safar;
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

Endpoint place(const std::string& id) {
    const auto* p = ctx().dataset->place(id);
    EXPECT_NE(p, nullptr) << id;
    return {Endpoint::Type::Place, static_cast<std::size_t>(p - ctx().dataset->places().data())};
}

Endpoint station(const std::string& id) {
    const auto* s = ctx().dataset->station(id);
    EXPECT_NE(s, nullptr) << id;
    return {Endpoint::Type::Station, static_cast<std::size_t>(s - ctx().dataset->stations().data())};
}

journey::JourneyRequest request(Endpoint from, Endpoint to, int hour = 10) {
    journey::JourneyRequest r;
    r.from = from;
    r.to = to;
    r.hour = hour;
    return r;
}

journey::ModeSet only(bool walk, bool metro, bool cab, bool autorickshaw, bool bike) {
    return {walk, metro, cab, autorickshaw, bike};
}

bool has_tag(const journey::Route& r, const std::string& tag) {
    return std::find(r.tags.begin(), r.tags.end(), tag) != r.tags.end();
}

bool has_leg(const search::Itinerary& it, search::Leg::Kind kind) {
    return std::any_of(it.legs.begin(), it.legs.end(), [&](const search::Leg& l) { return l.kind == kind; });
}

// Leg times add up to the total, and leg + ticket fares add up to the fare.
void expect_consistent(const search::Itinerary& it) {
    double minutes = 0, fare = 0;
    for (const auto& l : it.legs) {
        minutes += l.total_min();
        fare += l.fare_rs;
    }
    for (const auto& t : it.tickets) fare += t.fare_rs;
    EXPECT_NEAR(minutes, it.total_min, 1e-6);
    EXPECT_NEAR(fare, it.fare_rs, 1e-6);
}

}  // namespace

// ===== Fares and config =====

TEST(Fares, DmrcSlabsMatchTheOfficialChart) {
    const auto& t = ctx().transport->fare_table("DMRC");
    EXPECT_TRUE(t.verified);
    EXPECT_EQ(t.fare(1.0, false), 11);
    EXPECT_EQ(t.fare(2.0, false), 11);
    EXPECT_EQ(t.fare(2.1, false), 21);
    EXPECT_EQ(t.fare(12.0, false), 32);
    EXPECT_EQ(t.fare(20.0, false), 43);
    EXPECT_EQ(t.fare(45.0, false), 64);
    EXPECT_EQ(t.fare(4.0, true), 11);   // Sunday: 2-5 km slab is ₹11
    EXPECT_EQ(t.fare(45.0, true), 54);
}

TEST(Fares, RoadFaresRespectMinimums) {
    const auto& cfg = *ctx().transport;
    EXPECT_EQ(cfg.road_fare(graph::RoadMode::Cab, 0.5, 2), 90);  // cab minimum fare
    EXPECT_GE(cfg.road_fare(graph::RoadMode::Auto, 1.0, 3), 30);
    // Longer trips cost more, and a cab costs more than a bike taxi.
    EXPECT_GT(cfg.road_fare(graph::RoadMode::Cab, 20, 45), cfg.road_fare(graph::RoadMode::Cab, 10, 25));
    EXPECT_GT(cfg.road_fare(graph::RoadMode::Cab, 10, 25), cfg.road_fare(graph::RoadMode::Bike, 10, 25));
}

TEST(Fares, ValueOfTimeSpansTheConfiguredRange) {
    const auto& cfg = *ctx().transport;
    EXPECT_DOUBLE_EQ(cfg.value_of_time(0), cfg.vot_min);
    EXPECT_DOUBLE_EQ(cfg.value_of_time(1), cfg.vot_max);
    EXPECT_LT(cfg.value_of_time(0.3), cfg.value_of_time(0.7));
}

TEST(Fares, AirportExpressIsItsOwnTicketZone) {
    EXPECT_EQ(fares::fare_zone(*ctx().dataset->line("airport-express")), "DMRC-AE");
    EXPECT_EQ(fares::fare_zone(*ctx().dataset->line("blue")), "DMRC");
    EXPECT_EQ(fares::fare_zone(*ctx().dataset->line("aqua")), "NMRC");
}

TEST(RoadMatrix, HasRealRoadDataForBennett) {
    auto leg = ctx().roads->lookup("p:bennett-university", "p:hauz-khas-village");
    ASSERT_TRUE(leg);
    EXPECT_TRUE(leg->from_osrm);
    EXPECT_NEAR(leg->km, 50, 10);
    auto back = ctx().roads->lookup("p:hauz-khas-village", "p:bennett-university");
    ASSERT_TRUE(back);  // symmetric lookup
}

// ===== Yen's k shortest paths =====

TEST(Yen, PathsAreSortedDistinctAndLoopless) {
    const auto& g = *ctx().transit;
    const auto s = *g.find("kashmere-gate"), t = *g.find("botanical-garden");
    auto paths = search::k_shortest_paths(g, s, t, 6);
    ASSERT_EQ(paths.size(), 6u);
    EXPECT_NEAR(paths[0].cost, search::a_star(g, s, t).cost, 1e-9);
    std::set<std::vector<graph::EdgeId>> distinct;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i > 0) { EXPECT_GE(paths[i].cost, paths[i - 1].cost - 1e-9); }
        EXPECT_TRUE(distinct.insert(paths[i].path).second);
        std::set<graph::NodeId> visited{s};
        for (auto e : paths[i].path) EXPECT_TRUE(visited.insert(g.edge(e).to).second) << "loop in path " << i;
        EXPECT_EQ(g.edge(paths[i].path.back()).to, t);
    }
}

// ===== Journey planner =====

TEST(Planner, BennettToHauzKhasVillageGivesMixedRoutes) {
    auto r = ctx().planner->plan(request(place("bennett-university"), place("hauz-khas-village")));
    ASSERT_GE(r.routes.size(), 3u);
    EXPECT_TRUE(has_tag(r.routes.front(), "recommended"));

    bool mixed = false;
    std::set<std::string> tags;
    double min_time = 1e9, min_fare = 1e9;
    for (const auto& route : r.routes) {
        expect_consistent(route.itinerary);
        mixed |= has_leg(route.itinerary, search::Leg::Kind::Road) && has_leg(route.itinerary, search::Leg::Kind::Metro);
        tags.insert(route.tags.begin(), route.tags.end());
        min_time = std::min(min_time, route.itinerary.total_min);
        min_fare = std::min(min_fare, route.itinerary.fare_rs);
    }
    EXPECT_TRUE(mixed) << "expected at least one cab/auto/bike + metro route";
    for (const char* t : {"recommended", "fastest", "cheapest"}) EXPECT_TRUE(tags.count(t)) << t;
    for (const auto& route : r.routes) {
        if (has_tag(route, "fastest")) { EXPECT_DOUBLE_EQ(route.itinerary.total_min, min_time); }
        if (has_tag(route, "cheapest")) { EXPECT_DOUBLE_EQ(route.itinerary.fare_rs, min_fare); }
    }
}

TEST(Planner, RoadLegsListEveryVehicleOption) {
    auto r = ctx().planner->plan(request(place("india-gate"), place("hauz-khas-village")));
    bool checked = false;
    for (const auto& route : r.routes) {
        for (std::size_t i = 0; i < route.itinerary.legs.size(); ++i) {
            if (route.itinerary.legs[i].kind != search::Leg::Kind::Road) continue;
            const auto& opts = route.road_options[i];
            EXPECT_GE(opts.size(), 2u);  // ~10 km: cab, auto and bike all qualify
            EXPECT_TRUE(std::is_sorted(opts.begin(), opts.end(),
                                       [](const auto& a, const auto& b) { return a.fare_rs < b.fare_rs; }));
            checked = true;
        }
    }
    EXPECT_TRUE(checked);
}

TEST(Planner, StationToStationIsPlainMetroWithOneTicket) {
    auto req = request(station("rajiv-chowk"), station("hauz-khas"));
    req.preference = 0.2;
    auto r = ctx().planner->plan(req);
    const auto& best = r.routes.front().itinerary;
    ASSERT_EQ(best.legs.size(), 1u);
    EXPECT_EQ(best.legs[0].kind, search::Leg::Kind::Metro);
    ASSERT_EQ(best.tickets.size(), 1u);
    EXPECT_EQ(best.tickets[0].zone, "DMRC");
    EXPECT_EQ(best.fare_rs, ctx().transport->fare_table("DMRC").fare(best.tickets[0].distance_km, false));
}

TEST(Planner, AquaToBlueLineNeedsTwoTickets) {
    auto req = request(station("knowledge-park-ii"), station("rajiv-chowk"));
    req.modes = only(true, true, false, false, false);
    auto r = ctx().planner->plan(req);
    const auto& it = r.routes.front().itinerary;
    ASSERT_EQ(it.tickets.size(), 2u);
    EXPECT_EQ(it.tickets[0].zone, "NMRC");
    EXPECT_EQ(it.tickets[1].zone, "DMRC");
}

TEST(Planner, ModeFilterIsRespected) {
    auto req = request(place("hauz-khas-village"), place("india-gate"));
    req.modes = only(true, true, false, false, false);
    for (const auto& route : ctx().planner->plan(req).routes) {
        EXPECT_FALSE(has_leg(route.itinerary, search::Leg::Kind::Road));
    }

    // Bennett is ~6 km from the nearest station: walking + metro alone can't get there.
    auto far = request(place("bennett-university"), place("hauz-khas-village"));
    far.modes = only(true, true, false, false, false);
    EXPECT_THROW(ctx().planner->plan(far), journey::PlanError);
}

TEST(Planner, PeakTrafficSlowsCabs) {
    auto night = request(place("bennett-university"), place("connaught-place"), 3);
    auto peak = request(place("bennett-university"), place("connaught-place"), 9);
    night.modes = peak.modes = only(false, false, true, false, false);
    const double t_night = ctx().planner->plan(night).routes.front().itinerary.total_min;
    const double t_peak = ctx().planner->plan(peak).routes.front().itinerary.total_min;
    EXPECT_GT(t_peak, t_night * 1.4);
}

TEST(Planner, SundayMetroIsCheaper) {
    auto weekday = request(station("rajiv-chowk"), station("hauz-khas"));
    auto sunday = weekday;
    sunday.sunday = true;
    weekday.modes = sunday.modes = only(true, true, false, false, false);
    EXPECT_LT(ctx().planner->plan(sunday).routes.front().itinerary.fare_rs,
              ctx().planner->plan(weekday).routes.front().itinerary.fare_rs);
}

TEST(Planner, PreferenceSliderTradesMoneyForTime) {
    auto cheap = request(place("bennett-university"), place("hauz-khas-village"));
    auto fast = cheap;
    cheap.preference = 0;
    fast.preference = 1;
    const auto cheap_result = ctx().planner->plan(cheap);
    const auto fast_result = ctx().planner->plan(fast);
    const auto& c = cheap_result.routes.front().itinerary;
    const auto& f = fast_result.routes.front().itinerary;
    EXPECT_LE(c.fare_rs, f.fare_rs);
    EXPECT_LE(f.total_min, c.total_min);
}

TEST(Planner, RejectsSameEndpoints) {
    EXPECT_THROW(ctx().planner->plan(request(place("india-gate"), place("india-gate"))), journey::PlanError);
}

TEST(Planner, NoNearDuplicateRoutes) {
    auto r = ctx().planner->plan(request(place("bennett-university"), place("hauz-khas-village"), 9));
    for (std::size_t i = 0; i < r.routes.size(); ++i) {
        for (std::size_t j = i + 1; j < r.routes.size(); ++j) {
            const auto& a = r.routes[i].itinerary;
            const auto& b = r.routes[j].itinerary;
            const bool same_lines = a.legs.size() == b.legs.size() &&
                                    std::equal(a.legs.begin(), a.legs.end(), b.legs.begin(), [](const auto& x, const auto& y) {
                                        return x.kind == y.kind && x.line == y.line;
                                    });
            EXPECT_FALSE(same_lines && std::abs(a.total_min - b.total_min) <= 10 && std::abs(a.fare_rs - b.fare_rs) <= 20)
                << "routes " << i << " and " << j << " are near-duplicates";
        }
    }
}

TEST(Planner, DirectCabIsAlwaysShownAndCanBeMostExpensive) {
    // At 6 pm a cab to the airport is slower and dearer than bike + Airport Express,
    // but users still want to see what it would cost.
    auto r = ctx().planner->plan(request(place("india-gate"), place("igi-airport-terminal-3"), 18));
    const auto direct = std::find_if(r.routes.begin(), r.routes.end(), [](const journey::Route& x) {
        return x.itinerary.legs.size() == 1 && x.itinerary.legs[0].kind == search::Leg::Kind::Road;
    });
    ASSERT_NE(direct, r.routes.end());
    EXPECT_TRUE(has_tag(*direct, "most_expensive"));
    for (const auto& route : r.routes) {
        if (has_tag(route, "recommended")) { EXPECT_FALSE(route.dominated); }
    }
}

TEST(Planner, NobodyWalksIntoTheAirportFromOutside) {
    auto req = request(place("india-gate"), place("igi-airport-terminal-3"), 18);
    req.preference = 0;
    for (const auto& route : ctx().planner->plan(req).routes) {
        const auto& last = route.itinerary.legs.back();
        if (last.kind == search::Leg::Kind::Walk) { EXPECT_LE(last.distance_km, 0.6 * 1.3 + 1e-9); }
    }
}
