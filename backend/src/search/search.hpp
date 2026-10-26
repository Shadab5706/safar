#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "graph/graph.hpp"

namespace safar::search {

enum class Algorithm { Bfs, Dfs, Ucs, Greedy, AStar };

inline constexpr Algorithm kAllAlgorithms[] = {Algorithm::Bfs, Algorithm::Dfs, Algorithm::Ucs, Algorithm::Greedy,
                                               Algorithm::AStar};

std::optional<Algorithm> parse_algorithm(std::string_view name);
const char* to_string(Algorithm a);

// Step cost of an edge: time_weight * minutes + money_weight * rupees, plus an
// optional penalty every time you get on a train. The first boarding is penalised
// too, which shifts every route by the same amount, so in effect it is a
// per-change-of-line penalty. money_weight = 1 / value-of-time turns rupees into
// minutes, so one number ranks "₹40 cheaper" against "12 minutes slower".
struct CostModel {
    double time_weight = 1.0;
    double money_weight = 0.0;
    double boarding_penalty = 0.0;

    double edge_cost(const graph::Edge& e) const;
};

struct SearchOptions {
    CostModel cost;
    // Speed used by the A*/Greedy heuristic. 0 = graph.max_speed_kmh(), which is
    // admissible. A lower speed overestimates (inadmissible): faster but may miss
    // the best route. Exposed so the Algorithm Lab can show that trade-off.
    double heuristic_speed_kmh = 0;
    bool record_expansion = true;
    std::size_t max_recorded = 20000;
    // Optional masks (size = node/edge count, non-zero = blocked). Used by Yen's
    // algorithm, by mode filters ("no metro") and later by line closures.
    const std::vector<char>* blocked_nodes = nullptr;
    const std::vector<char>* blocked_edges = nullptr;
};

struct SearchStats {
    std::size_t nodes_expanded = 0;   // taken off the frontier and processed
    std::size_t nodes_generated = 0;  // put on the frontier
    std::size_t max_frontier = 0;     // peak frontier size (memory use)
    double runtime_us = 0;
};

struct SearchResult {
    Algorithm algorithm = Algorithm::Bfs;
    bool found = false;
    double cost = 0;                      // path cost under the CostModel (same for every algorithm)
    std::vector<graph::EdgeId> path;      // edges from start to goal, in order
    std::vector<graph::NodeId> expansion_order;
    SearchStats stats;
};

// h(n) = time_weight * straight_line_km(n, goal) / speed * 60.
class Heuristic {
public:
    Heuristic(const graph::Graph& g, graph::NodeId goal, const SearchOptions& opt);
    double operator()(graph::NodeId n) const;
    double speed_kmh() const { return speed_kmh_; }

private:
    const graph::Graph& g_;
    geo::LatLng goal_;
    double speed_kmh_;
    double time_weight_;
};

SearchResult breadth_first(const graph::Graph& g, graph::NodeId start, graph::NodeId goal, const SearchOptions& opt = {});
SearchResult depth_first(const graph::Graph& g, graph::NodeId start, graph::NodeId goal, const SearchOptions& opt = {});
SearchResult uniform_cost(const graph::Graph& g, graph::NodeId start, graph::NodeId goal, const SearchOptions& opt = {});
SearchResult greedy_best_first(const graph::Graph& g, graph::NodeId start, graph::NodeId goal, const SearchOptions& opt = {});
SearchResult a_star(const graph::Graph& g, graph::NodeId start, graph::NodeId goal, const SearchOptions& opt = {});

// Runs one algorithm and fills stats.runtime_us.
SearchResult run(Algorithm a, const graph::Graph& g, graph::NodeId start, graph::NodeId goal,
                 const SearchOptions& opt = {});

}  // namespace safar::search
