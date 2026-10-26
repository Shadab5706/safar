#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "data/dataset.hpp"
#include "graph/graph.hpp"

namespace safar::fares {
struct TransportConfig;
}

namespace safar::search {

// One human-readable step of a route. A metro leg covers everything from entering
// (or changing onto) a line to leaving it; consecutive rides are merged into it.
struct Leg {
    enum class Kind { Metro, Transfer, Walk, Road };
    Kind kind = Kind::Metro;
    graph::RoadMode road_mode = graph::RoadMode::Cab;  // road legs
    std::size_t line = graph::kNone;                   // metro legs
    graph::NodeId from = 0;
    graph::NodeId to = 0;
    std::vector<graph::NodeId> nodes;  // every node passed, from..to (for drawing the route)
    int stops = 0;
    double entry_min = 0;  // street -> platform
    double wait_min = 0;   // waiting for the train / the cab to arrive
    double ride_min = 0;
    double walk_min = 0;   // transfer walk or walking leg
    double exit_min = 0;   // platform -> street
    double distance_km = 0;
    double fare_rs = 0;   // road legs; metro fares live on the ticket
    int ticket = -1;      // metro legs: index into Itinerary::tickets
    bool road_from_osrm = false;

    double total_min() const { return entry_min + wait_min + ride_min + walk_min + exit_min; }
};

// One metro ticket: everything ridden between entering and leaving one operator's
// paid area. Blue -> Yellow at Rajiv Chowk is one DMRC ticket; Aqua -> Blue via the
// Sector 51/52 walkway is two (NMRC, then DMRC).
struct Ticket {
    std::string zone;
    graph::NodeId from = 0;
    graph::NodeId to = 0;
    double distance_km = 0;
    double fare_rs = 0;
    bool verified = false;  // fare chart checked against the operator's official one
};

struct Itinerary {
    std::vector<Leg> legs;
    std::vector<Ticket> tickets;
    double total_min = 0;
    double distance_km = 0;
    double fare_rs = 0;
    double walk_min = 0;
    int transfers = 0;  // number of times you change trains (metro legs - 1)
};

// Groups the edges of a search path into legs.
Itinerary build_itinerary(const graph::Graph& g, const std::vector<graph::EdgeId>& path);

// Splits metro legs into tickets, prices them with the exact slab fares and adds up
// the total fare (road legs rounded to whole rupees).
void price_itinerary(Itinerary& it, const data::Dataset& ds, const fares::TransportConfig& cfg, bool sunday);

// {"type": "station"|"place", "id", "name", "lat", "lng"} for any node.
nlohmann::json node_ref(const graph::Graph& g, const data::Dataset& ds, graph::NodeId n);

nlohmann::json to_json(const Itinerary& it, const graph::Graph& g, const data::Dataset& ds);

}  // namespace safar::search
