#pragma once

#include <vector>

#include "search/search.hpp"

namespace safar::search {

// Yen's algorithm (1971): the k cheapest loopless paths from start to goal,
// cheapest first. Path i+1 is found by taking each node of path i as a "spur",
// keeping the route up to it (the root), blocking the edges that earlier paths
// with the same root used next, and searching (with A*) from the spur to the goal.
//
// Respects opt.blocked_nodes / opt.blocked_edges as a base mask (e.g. "no metro").
std::vector<SearchResult> k_shortest_paths(const graph::Graph& g, graph::NodeId start, graph::NodeId goal,
                                           std::size_t k, const SearchOptions& opt = {});

}  // namespace safar::search
