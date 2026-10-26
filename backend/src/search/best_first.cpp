// Best-first search (AIMA 4th ed., fig. 3.7) and the three algorithms built on it.
// They differ only in the evaluation function f(n) that orders the frontier:
//   Uniform-cost search   f(n) = g(n)          optimal, explores in every direction
//   Greedy best-first     f(n) = h(n)          fast, not optimal
//   A*                    f(n) = g(n) + h(n)   optimal when h is admissible

#include <algorithm>
#include <limits>
#include <queue>
#include <vector>

#include "search/detail.hpp"
#include "search/search.hpp"

namespace safar::search {

using graph::EdgeId;
using graph::NodeId;

namespace {

enum class Priority { G, H, GPlusH };

SearchResult best_first(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt, Priority mode,
                        Algorithm algo) {
    SearchResult r;
    r.algorithm = algo;
    const Heuristic h(g, goal, opt);

    struct Entry {
        double f;
        double g;
        std::uint64_t seq;  // insertion order: ties go to the older entry, keeps runs deterministic
        NodeId node;
    };
    auto worse = [](const Entry& a, const Entry& b) { return a.f != b.f ? a.f > b.f : a.seq > b.seq; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(worse)> frontier(worse);

    // reached[n] = cheapest g found so far; a node is re-added when a cheaper path appears.
    std::vector<double> reached(g.node_count(), std::numeric_limits<double>::infinity());
    std::vector<EdgeId> parent(g.node_count(), detail::kNoEdge);
    std::uint64_t seq = 0;

    auto f_of = [&](double gn, NodeId n) {
        switch (mode) {
            case Priority::G: return gn;
            case Priority::H: return h(n);
            case Priority::GPlusH: return gn + h(n);
        }
        return gn;
    };

    reached[start] = 0;
    frontier.push({f_of(0, start), 0, seq++, start});
    r.stats.nodes_generated = 1;

    while (!frontier.empty()) {
        r.stats.max_frontier = std::max(r.stats.max_frontier, frontier.size());
        const Entry top = frontier.top();
        frontier.pop();
        if (top.g > reached[top.node]) continue;  // stale entry: a cheaper path was found later

        ++r.stats.nodes_expanded;
        detail::record(r, opt, top.node);

        if (top.node == goal) {  // goal test on expansion, needed for optimality
            if (goal == start) {
                r.found = true;
            } else {
                detail::reconstruct(g, parent, start, goal, opt.cost, r);
            }
            return r;
        }

        for (EdgeId e : g.out_edges(top.node)) {
            const auto& edge = g.edge(e);
            const double child_g = top.g + opt.cost.edge_cost(edge);
            if (child_g < reached[edge.to]) {
                reached[edge.to] = child_g;
                parent[edge.to] = e;
                frontier.push({f_of(child_g, edge.to), child_g, seq++, edge.to});
                ++r.stats.nodes_generated;
            }
        }
    }
    return r;
}

}  // namespace

SearchResult uniform_cost(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt) {
    return best_first(g, start, goal, opt, Priority::G, Algorithm::Ucs);
}

SearchResult greedy_best_first(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt) {
    return best_first(g, start, goal, opt, Priority::H, Algorithm::Greedy);
}

SearchResult a_star(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt) {
    return best_first(g, start, goal, opt, Priority::GPlusH, Algorithm::AStar);
}

}  // namespace safar::search
