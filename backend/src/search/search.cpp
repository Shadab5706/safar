#include "search/search.hpp"

#include <chrono>

namespace safar::search {

std::optional<Algorithm> parse_algorithm(std::string_view name) {
    if (name == "bfs") return Algorithm::Bfs;
    if (name == "dfs") return Algorithm::Dfs;
    if (name == "ucs") return Algorithm::Ucs;
    if (name == "greedy") return Algorithm::Greedy;
    if (name == "astar") return Algorithm::AStar;
    return std::nullopt;
}

const char* to_string(Algorithm a) {
    switch (a) {
        case Algorithm::Bfs: return "bfs";
        case Algorithm::Dfs: return "dfs";
        case Algorithm::Ucs: return "ucs";
        case Algorithm::Greedy: return "greedy";
        case Algorithm::AStar: return "astar";
    }
    return "bfs";
}

double CostModel::edge_cost(const graph::Edge& e) const {
    double c = time_weight * e.time_min;
    if (e.kind == graph::EdgeKind::Board || e.kind == graph::EdgeKind::Transfer) c += boarding_penalty;
    return c;
}

Heuristic::Heuristic(const graph::Graph& g, graph::NodeId goal, const SearchOptions& opt)
    : g_(g),
      goal_(g.node(goal).pos),
      speed_kmh_(opt.heuristic_speed_kmh > 0 ? opt.heuristic_speed_kmh : g.max_speed_kmh()),
      time_weight_(opt.cost.time_weight) {}

double Heuristic::operator()(graph::NodeId n) const {
    if (speed_kmh_ <= 0) return 0;  // graph with no movement: h = 0 is always admissible
    return time_weight_ * geo::haversine_km(g_.node(n).pos, goal_) / speed_kmh_ * 60.0;
}

SearchResult run(Algorithm a, const graph::Graph& g, graph::NodeId start, graph::NodeId goal,
                 const SearchOptions& opt) {
    const auto t0 = std::chrono::steady_clock::now();
    SearchResult r;
    switch (a) {
        case Algorithm::Bfs: r = breadth_first(g, start, goal, opt); break;
        case Algorithm::Dfs: r = depth_first(g, start, goal, opt); break;
        case Algorithm::Ucs: r = uniform_cost(g, start, goal, opt); break;
        case Algorithm::Greedy: r = greedy_best_first(g, start, goal, opt); break;
        case Algorithm::AStar: r = a_star(g, start, goal, opt); break;
    }
    r.stats.runtime_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

}  // namespace safar::search
