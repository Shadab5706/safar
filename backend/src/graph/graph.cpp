#include "graph/graph.hpp"

#include <algorithm>
#include <stdexcept>

#include "fares/transport_config.hpp"

namespace safar::graph {

NodeId Graph::add_node(Node n) {
    const auto id = static_cast<NodeId>(nodes_.size());
    if (!index_.emplace(n.id, id).second) {
        throw std::runtime_error("duplicate graph node: " + n.id);
    }
    nodes_.push_back(std::move(n));
    out_.emplace_back();
    return id;
}

EdgeId Graph::add_edge(Edge e) {
    if (e.from >= nodes_.size() || e.to >= nodes_.size()) {
        throw std::out_of_range("edge endpoint out of range");
    }
    if (e.time_min <= 0) {
        throw std::invalid_argument("edge time must be positive: " + nodes_[e.from].id + " -> " + nodes_[e.to].id);
    }
    const double straight_km = geo::haversine_km(nodes_[e.from].pos, nodes_[e.to].pos);
    max_speed_kmh_ = std::max(max_speed_kmh_, straight_km / (e.time_min / 60.0));

    const auto id = static_cast<EdgeId>(edges_.size());
    out_[e.from].push_back(id);
    edges_.push_back(e);
    return id;
}

std::optional<NodeId> Graph::find(std::string_view id) const {
    auto it = index_.find(std::string(id));
    if (it == index_.end()) return std::nullopt;
    return it->second;
}

Graph build_transit_graph(const data::Dataset& ds, const TransitParams& params, const fares::TransportConfig* fares) {
    Graph g;
    const auto& stations = ds.stations();
    const auto& lines = ds.lines();

    // Linearised fare per line (0 when no fare config is given, e.g. in unit tests).
    std::vector<double> board_rs(lines.size(), 0), per_km_rs(lines.size(), 0);
    std::vector<std::string> zone(lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        zone[i] = fares::fare_zone(lines[i]);
        if (fares) {
            const auto& table = fares->fare_table(zone[i]);
            board_rs[i] = table.base_fare();
            per_km_rs[i] = table.per_km();
        }
    }

    std::unordered_map<std::string, std::size_t> station_idx, line_idx;
    for (std::size_t i = 0; i < stations.size(); ++i) station_idx[stations[i].id] = i;
    for (std::size_t i = 0; i < lines.size(); ++i) line_idx[lines[i].id] = i;

    auto platform_id = [](const std::string& station, const std::string& line) { return station + "@" + line; };

    // Gates.
    std::vector<NodeId> gate(stations.size());
    for (std::size_t i = 0; i < stations.size(); ++i) {
        gate[i] = g.add_node({stations[i].id, NodeKind::Gate, stations[i].pos, i, kNone});
    }

    // Platforms + board/alight edges.
    for (std::size_t li = 0; li < lines.size(); ++li) {
        const auto& line = lines[li];
        for (const auto& sid : line.stations) {
            const auto pid = platform_id(sid, line.id);
            if (g.find(pid)) continue;  // loop lines list a station twice
            const std::size_t si = station_idx.at(sid);
            const NodeId p = g.add_node({pid, NodeKind::Platform, stations[si].pos, si, li});
            g.add_edge({gate[si], p, EdgeKind::Board, params.entry_min + line.avg_wait_min, line.avg_wait_min, 0, li,
                        board_rs[li]});
            g.add_edge({p, gate[si], EdgeKind::Alight, params.exit_min, 0, 0, kNone});
        }
    }

    // Rides, both directions.
    for (const auto& s : ds.segments()) {
        const std::size_t li = line_idx.at(s.line);
        const NodeId a = *g.find(platform_id(s.a, s.line));
        const NodeId b = *g.find(platform_id(s.b, s.line));
        const double rs = per_km_rs[li] * s.distance_km;
        g.add_edge({a, b, EdgeKind::Ride, s.time_min, 0, s.distance_km, li, rs});
        g.add_edge({b, a, EdgeKind::Ride, s.time_min, 0, s.distance_km, li, rs});
    }

    // Transfers and walking links.
    for (const auto& t : ds.transfers()) {
        if (t.kind == data::Transfer::Kind::InStation) {
            // Invariant: changing line inside a station must be quicker than leaving
            // and coming back in, otherwise routes would "exit and re-enter" (and in
            // reality pay a second fare).
            if (t.walk_min >= params.entry_min + params.exit_min) {
                throw std::runtime_error("transfer at " + t.station + " (" + std::to_string(t.walk_min) +
                                         " min) is slower than exit + re-entry");
            }
            const auto& st = stations[station_idx.at(t.station)];
            for (const auto& from_line : st.lines) {
                for (const auto& to_line : st.lines) {
                    if (from_line == to_line) continue;
                    const std::size_t from_li = line_idx.at(from_line);
                    const std::size_t to_li = line_idx.at(to_line);
                    const double wait = lines[to_li].avg_wait_min;
                    // Same operator = same ticket. Another operator = a new ticket.
                    const double rs = zone[from_li] == zone[to_li] ? 0 : board_rs[to_li];
                    g.add_edge({*g.find(platform_id(st.id, from_line)), *g.find(platform_id(st.id, to_line)),
                                EdgeKind::Transfer, t.walk_min + wait, wait, 0, to_li, rs});
                }
            }
        } else {
            const NodeId a = gate[station_idx.at(t.a)];
            const NodeId b = gate[station_idx.at(t.b)];
            g.add_edge({a, b, EdgeKind::Walk, t.walk_min, 0, t.distance_km, kNone});
            g.add_edge({b, a, EdgeKind::Walk, t.walk_min, 0, t.distance_km, kNone});
        }
    }
    return g;
}

const char* to_string(EdgeKind k) {
    switch (k) {
        case EdgeKind::Ride: return "ride";
        case EdgeKind::Board: return "board";
        case EdgeKind::Alight: return "alight";
        case EdgeKind::Transfer: return "transfer";
        case EdgeKind::Walk: return "walk";
        case EdgeKind::Road: return "road";
    }
    return "ride";
}

const char* to_string(RoadMode m) {
    switch (m) {
        case RoadMode::Cab: return "cab";
        case RoadMode::Auto: return "auto";
        case RoadMode::Bike: return "bike";
    }
    return "cab";
}

}  // namespace safar::graph
