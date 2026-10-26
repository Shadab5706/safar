#pragma once

// Helpers shared by the search algorithms. Not part of the public API.

#include <algorithm>
#include <vector>

#include "search/search.hpp"

namespace safar::search::detail {

inline constexpr graph::EdgeId kNoEdge = static_cast<graph::EdgeId>(-1);

// Walks parent edges back from goal to start and fills result.path + result.cost.
inline void reconstruct(const graph::Graph& g, const std::vector<graph::EdgeId>& parent, graph::NodeId start,
                        graph::NodeId goal, const CostModel& cost, SearchResult& result) {
    result.found = true;
    result.path.clear();
    for (graph::NodeId n = goal; n != start;) {
        const graph::EdgeId e = parent[n];
        result.path.push_back(e);
        n = g.edge(e).from;
    }
    std::reverse(result.path.begin(), result.path.end());
    result.cost = 0;
    for (auto e : result.path) result.cost += cost.edge_cost(g.edge(e));
}

inline bool is_blocked(const SearchOptions& opt, graph::EdgeId e, graph::NodeId to) {
    return (opt.blocked_edges && (*opt.blocked_edges)[e]) || (opt.blocked_nodes && (*opt.blocked_nodes)[to]);
}

inline void record(SearchResult& r, const SearchOptions& opt, graph::NodeId n) {
    if (opt.record_expansion && r.expansion_order.size() < opt.max_recorded) r.expansion_order.push_back(n);
}

}  // namespace safar::search::detail
