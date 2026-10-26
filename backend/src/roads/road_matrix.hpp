#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace safar::roads {

struct RoadLeg {
    double km = 0;
    double free_flow_min = 0;  // no traffic; the planner applies the time-of-day factor
    bool from_osrm = false;
};

// Road distances and free-flow drive times prefetched from OSRM
// (scripts/prefetch_road_matrix.py). Keys are "p:<place id>" and "s:<station id>".
class RoadMatrix {
public:
    // A missing file gives an empty matrix: every lookup then falls back to estimates.
    static RoadMatrix load(const std::filesystem::path& file);

    // Looks up a -> b, then b -> a (roads are treated as symmetric).
    std::optional<RoadLeg> lookup(const std::string& a, const std::string& b) const;
    std::size_t size() const { return pairs_.size(); }

private:
    std::unordered_map<std::string, std::pair<float, float>> pairs_;  // "a|b" -> (km, min)
};

inline std::string place_key(const std::string& id) { return "p:" + id; }
inline std::string station_key(const std::string& id) { return "s:" + id; }

}  // namespace safar::roads
