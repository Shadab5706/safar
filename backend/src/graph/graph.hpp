#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "data/dataset.hpp"
#include "util/geo.hpp"

namespace safar::graph {

using NodeId = std::uint32_t;
using EdgeId = std::uint32_t;
inline constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

// Gate     = a station at street level ("rajiv-chowk"). Journeys start and end here.
// Platform = one line inside a station ("rajiv-chowk@yellow"). Trains run between platforms.
// Splitting them is what makes a change of line cost time: you can only get from the
// Yellow platform to the Blue platform through a Transfer edge (walk + wait).
enum class NodeKind { Gate, Platform };

enum class EdgeKind {
    Ride,      // platform -> next platform on the same line
    Board,     // gate -> platform: security check, stairs, wait for the train
    Alight,    // platform -> gate: stairs, exit gates
    Transfer,  // platform -> platform of another line in the same station
    Walk,      // gate -> gate of a nearby station (e.g. Noida Sector 52 <-> 51)
};

struct Node {
    std::string id;
    NodeKind kind = NodeKind::Gate;
    geo::LatLng pos;
    std::size_t station = kNone;  // index into Dataset::stations()
    std::size_t line = kNone;     // index into Dataset::lines(), platforms only
};

struct Edge {
    NodeId from = 0;
    NodeId to = 0;
    EdgeKind kind = EdgeKind::Ride;
    double time_min = 0;     // total time of the edge, including wait_min
    double wait_min = 0;     // the part spent waiting for a train
    double distance_km = 0;  // distance actually travelled (0 inside a station)
    std::size_t line = kNone;  // line ridden / boarded / transferred to
};

// Model constants for moving through a station. Kept here (not in the data files)
// because they belong to the routing model, not to the network.
struct TransitParams {
    double entry_min = 4.0;  // street -> platform: security check + stairs
    double exit_min = 2.5;   // platform -> street
};

class Graph {
public:
    NodeId add_node(Node n);
    EdgeId add_edge(Edge e);

    std::size_t node_count() const { return nodes_.size(); }
    std::size_t edge_count() const { return edges_.size(); }
    const Node& node(NodeId id) const { return nodes_[id]; }
    const Edge& edge(EdgeId id) const { return edges_[id]; }
    const std::vector<EdgeId>& out_edges(NodeId id) const { return out_[id]; }
    std::optional<NodeId> find(std::string_view id) const;

    // Highest straight-line speed any edge achieves (km/h). Dividing straight-line
    // distance by this speed can never overestimate the remaining time, and by the
    // triangle inequality the resulting heuristic is consistent, not just admissible.
    double max_speed_kmh() const { return max_speed_kmh_; }

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::vector<std::vector<EdgeId>> out_;
    std::unordered_map<std::string, NodeId> index_;
    double max_speed_kmh_ = 0;
};

// Builds the metro/RRTS network as a directed graph from the dataset.
// Throws std::runtime_error if a model invariant is broken (see graph.cpp).
Graph build_transit_graph(const data::Dataset& ds, const TransitParams& params = {});

const char* to_string(EdgeKind k);

}  // namespace safar::graph
