#include "search/yen.hpp"

#include <algorithm>
#include <set>

namespace safar::search {

using graph::EdgeId;
using graph::NodeId;

namespace {

double path_cost(const graph::Graph& g, const std::vector<EdgeId>& path, const CostModel& cost) {
    double c = 0;
    for (auto e : path) c += cost.edge_cost(g.edge(e));
    return c;
}

// Sets mask[i] = 1 and remembers i so it can be undone, leaving base-mask entries alone.
struct TempMask {
    std::vector<char> mask;
    std::vector<std::size_t> touched;

    void block(std::size_t i) {
        if (!mask[i]) {
            mask[i] = 1;
            touched.push_back(i);
        }
    }
    void reset() {
        for (auto i : touched) mask[i] = 0;
        touched.clear();
    }
};

}  // namespace

std::vector<SearchResult> k_shortest_paths(const graph::Graph& g, NodeId start, NodeId goal, std::size_t k,
                                           const SearchOptions& opt) {
    std::vector<SearchResult> accepted;
    if (k == 0) return accepted;

    SearchOptions base = opt;
    base.record_expansion = false;
    auto first = a_star(g, start, goal, base);
    if (!first.found || first.path.empty()) {
        if (first.found) accepted.push_back(std::move(first));
        return accepted;
    }
    accepted.push_back(std::move(first));

    TempMask nodes{opt.blocked_nodes ? *opt.blocked_nodes : std::vector<char>(g.node_count(), 0), {}};
    TempMask edges{opt.blocked_edges ? *opt.blocked_edges : std::vector<char>(g.edge_count(), 0), {}};
    SearchOptions spur_opt = base;
    spur_opt.blocked_nodes = &nodes.mask;
    spur_opt.blocked_edges = &edges.mask;

    std::vector<SearchResult> candidates;  // "B" in Yen's paper
    std::set<std::vector<EdgeId>> seen{accepted.front().path};

    while (accepted.size() < k) {
        const auto& prev = accepted.back().path;
        std::vector<NodeId> prev_nodes{start};
        for (auto e : prev) prev_nodes.push_back(g.edge(e).to);

        for (std::size_t i = 0; i < prev.size(); ++i) {
            const NodeId spur = prev_nodes[i];
            const std::vector<EdgeId> root(prev.begin(), prev.begin() + static_cast<std::ptrdiff_t>(i));

            // Don't repeat an edge that an accepted path with this same root took next.
            for (const auto& a : accepted) {
                if (a.path.size() > i && std::equal(root.begin(), root.end(), a.path.begin())) {
                    edges.block(a.path[i]);
                }
            }
            // Keep paths loopless: the spur path may not revisit root nodes.
            for (std::size_t r = 0; r < i; ++r) nodes.block(prev_nodes[r]);

            auto spur_path = a_star(g, spur, goal, spur_opt);
            nodes.reset();
            edges.reset();
            if (!spur_path.found) continue;

            std::vector<EdgeId> total = root;
            total.insert(total.end(), spur_path.path.begin(), spur_path.path.end());
            if (!seen.insert(total).second) continue;

            SearchResult cand;
            cand.algorithm = Algorithm::AStar;
            cand.found = true;
            cand.cost = path_cost(g, total, opt.cost);
            cand.path = std::move(total);
            candidates.push_back(std::move(cand));
        }

        if (candidates.empty()) break;
        auto best = std::min_element(candidates.begin(), candidates.end(),
                                     [](const SearchResult& a, const SearchResult& b) { return a.cost < b.cost; });
        accepted.push_back(std::move(*best));
        candidates.erase(best);
    }
    return accepted;
}

}  // namespace safar::search
