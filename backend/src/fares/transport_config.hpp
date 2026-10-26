#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "data/dataset.hpp"
#include "graph/graph.hpp"

namespace safar::fares {

// Distance-slab fare chart of one metro operator ("fare zone").
struct FareTable {
    bool verified = false;  // true = checked against the operator's official chart
    std::vector<std::pair<double, double>> weekday;  // (up_to_km, fare); last up_to is +inf
    std::vector<std::pair<double, double>> sunday;   // empty = same as weekday

    double fare(double km, bool on_sunday) const;
    double base_fare() const;  // cheapest weekday slab
    double per_km() const;     // average rupees per km across the slabs (used by the search)
};

// Generic rate card for one road mode.
// fare = max(min_fare, base + per_km * max(0, km - base_km) + per_min * ride_min)
struct RoadModeParams {
    double base = 0, base_km = 0, per_km = 0, per_min = 0, min_fare = 0;
    double pickup_min = 0;    // average wait for the vehicle to arrive
    double speed_factor = 1;  // ride time relative to a car (auto slower, bike faster in traffic)
    double max_km = 0;         // longest whole trip this mode is offered for
    double max_access_km = 0;  // longest ride to or from a station
};

struct WalkParams {
    double speed_kmh = 4.5;
    double detour_factor = 1.3;
    double max_access_km = 2.0;
    double max_direct_km = 3.0;
    std::unordered_map<std::string, double> max_km_by_category;  // place category -> tighter limit
};

// Everything in backend/config/transport.json.
struct TransportConfig {
    std::unordered_map<std::string, FareTable> metro;
    std::array<RoadModeParams, 3> road{};  // indexed by graph::RoadMode
    WalkParams walk;
    double fallback_detour = 1.35;
    double fallback_speed_kmh = 32;
    std::size_t nearest_stations = 8;
    double line_radius_km = 35;
    std::array<double, 24> traffic{};
    double vot_min = 0.5;
    double vot_max = 15;

    static TransportConfig load(const std::filesystem::path& file);

    const FareTable& fare_table(const std::string& zone) const;  // throws if unknown
    const RoadModeParams& road_params(graph::RoadMode m) const { return road[static_cast<std::size_t>(m)]; }
    double road_fare(graph::RoadMode m, double km, double ride_min) const;

    // Rupees a minute of saved time is worth. preference 0 -> vot_min (cheapest),
    // 1 -> vot_max (fastest), geometric in between so the slider feels even.
    double value_of_time(double preference) const;
};

// The ticketing area a line belongs to: its operator, except the Airport Express,
// which DMRC runs as a separate paid area with its own fares.
std::string fare_zone(const data::Line& line);

}  // namespace safar::fares
