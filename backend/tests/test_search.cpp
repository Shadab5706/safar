#include <gtest/gtest.h>

#include <algorithm>
#include <random>

#include "data/dataset.hpp"
#include "graph/graph.hpp"
#include "search/itinerary.hpp"
#include "search/search.hpp"

using namespace safar;
using graph::EdgeKind;
using graph::Graph;
using graph::NodeId;
using search::Algorithm;

namespace {

// ---------- hand-made graphs ----------

NodeId add(Graph& g, const std::string& id, double lat, double lng) {
    return g.add_node({id, graph::NodeKind::Gate, {lat, lng}, graph::kNone, graph::kNone});
}

void link(Graph& g, NodeId a, NodeId b, double minutes) {
    g.add_edge({a, b, EdgeKind::Walk, minutes, 0, 0, graph::kNone});
}

// S --10--> G directly, or S -1-> A -1-> B -1-> G. G sits right next to S on the
// map, so the heuristic points straight at the expensive edge.
struct Trap {
    Graph g;
    NodeId s, a, b, goal;
    Trap() {
        s = add(g, "S", 28.00, 77.00);
        a = add(g, "A", 28.05, 77.00);
        b = add(g, "B", 28.05, 77.03);
        goal = add(g, "G", 28.00, 77.01);
        link(g, s, goal, 10);
        link(g, s, a, 1);
        link(g, a, b, 1);
        link(g, b, goal, 1);
    }
};

// ---------- real network ----------

struct Real {
    data::Dataset ds = data::Dataset::load(SAFAR_TEST_DATA_DIR);
    Graph g = graph::build_transit_graph(ds);
};

const Real& real() {
    static const Real r;
    return r;
}

NodeId gate(const std::string& station) { return *real().g.find(station); }

std::vector<std::pair<NodeId, NodeId>> random_station_pairs(int n, unsigned seed) {
    std::mt19937 rng(seed);
    const auto& st = real().ds.stations();
    std::uniform_int_distribution<std::size_t> pick(0, st.size() - 1);
    std::vector<std::pair<NodeId, NodeId>> out;
    while (static_cast<int>(out.size()) < n) {
        auto a = pick(rng), b = pick(rng);
        if (a != b) out.emplace_back(gate(st[a].id), gate(st[b].id));
    }
    return out;
}

// The path is a connected chain of edges from start to goal and its cost adds up.
void expect_valid_path(const Graph& g, const search::SearchResult& r, NodeId start, NodeId goal,
                       const search::CostModel& cost = {}) {
    ASSERT_TRUE(r.found);
    ASSERT_FALSE(r.path.empty());
    EXPECT_EQ(g.edge(r.path.front()).from, start);
    EXPECT_EQ(g.edge(r.path.back()).to, goal);
    double sum = 0;
    for (std::size_t i = 0; i < r.path.size(); ++i) {
        if (i > 0) EXPECT_EQ(g.edge(r.path[i - 1]).to, g.edge(r.path[i]).from);
        sum += cost.edge_cost(g.edge(r.path[i]));
    }
    EXPECT_NEAR(sum, r.cost, 1e-9);
}

}  // namespace

// ===== Hand-made graphs: known answers =====

TEST(SearchTrap, UcsAndAStarFindTheCheapRoute) {
    Trap t;
    for (auto algo : {Algorithm::Ucs, Algorithm::AStar}) {
        auto r = search::run(algo, t.g, t.s, t.goal);
        expect_valid_path(t.g, r, t.s, t.goal);
        EXPECT_DOUBLE_EQ(r.cost, 3.0) << search::to_string(algo);
        EXPECT_EQ(r.path.size(), 3u);
    }
}

TEST(SearchTrap, BfsFindsFewestEdgesNotCheapest) {
    Trap t;
    auto r = search::breadth_first(t.g, t.s, t.goal);
    expect_valid_path(t.g, r, t.s, t.goal);
    EXPECT_EQ(r.path.size(), 1u);
    EXPECT_DOUBLE_EQ(r.cost, 10.0);
}

TEST(SearchTrap, GreedyFollowsTheHeuristicIntoTheTrap) {
    Trap t;
    auto r = search::greedy_best_first(t.g, t.s, t.goal);
    expect_valid_path(t.g, r, t.s, t.goal);
    EXPECT_DOUBLE_EQ(r.cost, 10.0);  // G looks closest, so it is taken at once
}

TEST(SearchTrap, DfsFindsSomeRoute) {
    Trap t;
    auto r = search::depth_first(t.g, t.s, t.goal);
    expect_valid_path(t.g, r, t.s, t.goal);
    EXPECT_GE(r.cost, 3.0);
}

TEST(SearchEdgeCases, StartEqualsGoal) {
    Trap t;
    for (auto algo : search::kAllAlgorithms) {
        auto r = search::run(algo, t.g, t.s, t.s);
        EXPECT_TRUE(r.found) << search::to_string(algo);
        EXPECT_TRUE(r.path.empty());
        EXPECT_DOUBLE_EQ(r.cost, 0.0);
    }
}

TEST(SearchEdgeCases, UnreachableGoal) {
    Trap t;
    const NodeId island = add(t.g, "island", 28.2, 77.2);
    for (auto algo : search::kAllAlgorithms) {
        auto r = search::run(algo, t.g, t.s, island);
        EXPECT_FALSE(r.found) << search::to_string(algo);
        EXPECT_TRUE(r.path.empty());
        EXPECT_GT(r.stats.nodes_expanded, 0u);
    }
}

TEST(SearchEdgeCases, ParseAlgorithmNames) {
    for (auto algo : search::kAllAlgorithms) {
        EXPECT_EQ(search::parse_algorithm(search::to_string(algo)), algo);
    }
    EXPECT_FALSE(search::parse_algorithm("dijkstra"));
}

// ===== Real network: properties =====

TEST(TransitGraph, HasGatesPlatformsAndPositiveMaxSpeed) {
    const auto& g = real().g;
    EXPECT_GT(g.node_count(), real().ds.stations().size());  // gates + platforms
    ASSERT_TRUE(g.find("rajiv-chowk"));
    ASSERT_TRUE(g.find("rajiv-chowk@yellow"));
    ASSERT_TRUE(g.find("rajiv-chowk@blue"));
    // Namo Bharat is the fastest service; nothing should beat ~130 km/h straight-line.
    EXPECT_GT(g.max_speed_kmh(), 60.0);
    EXPECT_LT(g.max_speed_kmh(), 130.0);
}

TEST(TransitGraph, HeuristicIsConsistentOnEveryEdge) {
    const auto& g = real().g;
    search::SearchOptions opt;
    for (auto [unused, goal] : random_station_pairs(25, 7)) {
        search::Heuristic h(g, goal, opt);
        for (graph::EdgeId e = 0; e < g.edge_count(); ++e) {
            const auto& edge = g.edge(e);
            ASSERT_LE(h(edge.from), opt.cost.edge_cost(edge) + h(edge.to) + 1e-9)
                << g.node(edge.from).id << " -> " << g.node(edge.to).id;
        }
    }
}

TEST(RealQueries, AStarMatchesUcsOn100RandomQueries) {
    const auto& g = real().g;
    std::size_t ucs_expanded = 0, astar_expanded = 0;
    for (auto [s, t] : random_station_pairs(100, 42)) {
        auto ucs = search::uniform_cost(g, s, t);
        auto astar = search::a_star(g, s, t);
        expect_valid_path(g, ucs, s, t);
        expect_valid_path(g, astar, s, t);
        EXPECT_NEAR(astar.cost, ucs.cost, 1e-9) << g.node(s).id << " -> " << g.node(t).id;
        EXPECT_LE(astar.stats.nodes_expanded, ucs.stats.nodes_expanded);
        ucs_expanded += ucs.stats.nodes_expanded;
        astar_expanded += astar.stats.nodes_expanded;
    }
    EXPECT_LT(astar_expanded, ucs_expanded);  // the heuristic actually saves work overall
}

TEST(RealQueries, NonOptimalAlgorithmsNeverBeatUcs) {
    const auto& g = real().g;
    for (auto [s, t] : random_station_pairs(100, 99)) {
        const double best = search::uniform_cost(g, s, t).cost;
        for (auto algo : {Algorithm::Bfs, Algorithm::Dfs, Algorithm::Greedy}) {
            auto r = search::run(algo, g, s, t);
            expect_valid_path(g, r, s, t);
            EXPECT_GE(r.cost, best - 1e-9) << search::to_string(algo);
        }
    }
}

TEST(RealQueries, InadmissibleHeuristicExpandsLessButCanLoseOptimality) {
    const auto& g = real().g;
    search::SearchOptions fast;
    fast.heuristic_speed_kmh = 15;  // far slower than trains really go -> overestimates
    std::size_t admissible = 0, inadmissible = 0;
    for (auto [s, t] : random_station_pairs(100, 5)) {
        auto good = search::a_star(g, s, t);
        auto quick = search::a_star(g, s, t, fast);
        EXPECT_GE(quick.cost, good.cost - 1e-9);
        admissible += good.stats.nodes_expanded;
        inadmissible += quick.stats.nodes_expanded;
    }
    EXPECT_LT(inadmissible, admissible);
}

TEST(RealQueries, BoardingPenaltyNeverIncreasesChanges) {
    const auto& g = real().g;
    search::SearchOptions penal;
    penal.cost.boarding_penalty = 20;
    for (auto [s, t] : random_station_pairs(60, 11)) {
        auto plain = search::build_itinerary(g, search::a_star(g, s, t).path);
        auto fewer = search::build_itinerary(g, search::a_star(g, s, t, penal).path);
        EXPECT_LE(fewer.transfers, plain.transfers) << g.node(s).id << " -> " << g.node(t).id;
    }
}

// ===== Real network: routes a Delhi commuter would recognise =====

TEST(KnownRoutes, RajivChowkToHauzKhasIsOneYellowLineRide) {
    const auto& g = real().g;
    auto r = search::a_star(g, gate("rajiv-chowk"), gate("hauz-khas"));
    auto it = search::build_itinerary(g, r.path);
    ASSERT_EQ(it.legs.size(), 1u);
    EXPECT_EQ(real().ds.lines()[it.legs[0].line].id, "yellow");
    EXPECT_EQ(it.transfers, 0);
    EXPECT_GT(it.total_min, 18);
    EXPECT_LT(it.total_min, 35);
}

TEST(KnownRoutes, AquaLineToRajivChowkUsesTheSector52Walkway) {
    const auto& g = real().g;
    auto r = search::a_star(g, gate("noida-sector-51"), gate("rajiv-chowk"));
    auto it = search::build_itinerary(g, r.path);
    bool walked = false, rode_blue = false;
    for (const auto& leg : it.legs) {
        walked |= leg.kind == search::Leg::Kind::Walk;
        rode_blue |= leg.kind == search::Leg::Kind::Metro && real().ds.lines()[leg.line].id == "blue";
    }
    EXPECT_TRUE(walked);
    EXPECT_TRUE(rode_blue);
}

TEST(KnownRoutes, ItineraryTimesAddUp) {
    const auto& g = real().g;
    auto r = search::a_star(g, gate("kashmere-gate"), gate("dwarka-sector-21"));
    auto it = search::build_itinerary(g, r.path);
    double legs = 0;
    for (const auto& l : it.legs) legs += l.total_min();
    EXPECT_NEAR(legs, it.total_min, 1e-9);
    EXPECT_NEAR(it.total_min, r.cost, 1e-9);  // default cost model = minutes
    EXPECT_GE(it.transfers, 1);
}
