// Uninformed search: BFS and DFS. Neither looks at edge costs, so neither is
// guaranteed to find the cheapest route. They are here for comparison.

#include <algorithm>
#include <deque>
#include <vector>

#include "search/detail.hpp"
#include "search/search.hpp"

namespace safar::search {

using graph::EdgeId;
using graph::NodeId;

// Breadth-first graph search. Finds the route with the fewest edges.
// Goal test on generation (as in AIMA): BFS cannot do better by waiting.
SearchResult breadth_first(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt) {
    SearchResult r;
    r.algorithm = Algorithm::Bfs;
    if (start == goal) {
        r.found = true;
        return r;
    }

    std::vector<EdgeId> parent(g.node_count(), detail::kNoEdge);
    std::vector<bool> reached(g.node_count(), false);
    std::deque<NodeId> frontier{start};
    reached[start] = true;
    r.stats.nodes_generated = 1;

    while (!frontier.empty()) {
        r.stats.max_frontier = std::max(r.stats.max_frontier, frontier.size());
        const NodeId n = frontier.front();
        frontier.pop_front();
        ++r.stats.nodes_expanded;
        detail::record(r, opt, n);

        for (EdgeId e : g.out_edges(n)) {
            const NodeId child = g.edge(e).to;
            if (reached[child]) continue;
            reached[child] = true;
            parent[child] = e;
            ++r.stats.nodes_generated;
            if (child == goal) {
                detail::reconstruct(g, parent, start, goal, opt.cost, r);
                return r;
            }
            frontier.push_back(child);
        }
    }
    return r;
}

// Depth-first graph search with an explicit stack. Follows one branch as deep as
// it can before backtracking, so the route it returns is usually long and odd.
SearchResult depth_first(const graph::Graph& g, NodeId start, NodeId goal, const SearchOptions& opt) {
    SearchResult r;
    r.algorithm = Algorithm::Dfs;

    struct Entry {
        NodeId node;
        EdgeId via;
    };
    std::vector<EdgeId> parent(g.node_count(), detail::kNoEdge);
    std::vector<bool> explored(g.node_count(), false);
    std::vector<Entry> stack{{start, detail::kNoEdge}};
    r.stats.nodes_generated = 1;

    while (!stack.empty()) {
        r.stats.max_frontier = std::max(r.stats.max_frontier, stack.size());
        const Entry top = stack.back();
        stack.pop_back();
        if (explored[top.node]) continue;  // reached earlier along another branch
        explored[top.node] = true;
        parent[top.node] = top.via;
        ++r.stats.nodes_expanded;
        detail::record(r, opt, top.node);

        if (top.node == goal) {
            if (goal == start) {
                r.found = true;
            } else {
                detail::reconstruct(g, parent, start, goal, opt.cost, r);
            }
            return r;
        }

        // Push in reverse so the first listed neighbour is explored first.
        const auto& out = g.out_edges(top.node);
        for (auto it = out.rbegin(); it != out.rend(); ++it) {
            const NodeId child = g.edge(*it).to;
            if (!explored[child]) {
                stack.push_back({child, *it});
                ++r.stats.nodes_generated;
            }
        }
    }
    return r;
}

}  // namespace safar::search
