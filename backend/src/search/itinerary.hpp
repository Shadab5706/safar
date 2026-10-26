#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "data/dataset.hpp"
#include "graph/graph.hpp"

namespace safar::search {

// One human-readable step of a route. A metro leg covers everything from entering
// (or changing onto) a line to leaving it; consecutive rides are merged into it.
struct Leg {
    enum class Kind { Metro, Transfer, Walk };
    Kind kind = Kind::Metro;
    std::size_t line = graph::kNone;  // metro legs
    std::size_t from_station = graph::kNone;
    std::size_t to_station = graph::kNone;
    std::vector<std::size_t> stations;  // every station passed, from..to (for drawing the route)
    int stops = 0;
    double entry_min = 0;  // street -> platform (first leg after entering a station)
    double wait_min = 0;   // average wait for the train
    double ride_min = 0;
    double walk_min = 0;   // transfer walk or street walk
    double exit_min = 0;   // platform -> street
    double distance_km = 0;

    double total_min() const { return entry_min + wait_min + ride_min + walk_min + exit_min; }
};

struct Itinerary {
    std::vector<Leg> legs;
    double total_min = 0;
    double distance_km = 0;
    int transfers = 0;  // number of times you change trains (metro legs - 1)
};

// Groups the edges of a search path into legs. The path must start and end at gates.
Itinerary build_itinerary(const graph::Graph& g, const std::vector<graph::EdgeId>& path);

nlohmann::json to_json(const Itinerary& it, const data::Dataset& ds);

}  // namespace safar::search
