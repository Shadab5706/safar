#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "data/dataset.hpp"
#include "fares/transport_config.hpp"
#include "graph/graph.hpp"
#include "roads/road_matrix.hpp"
#include "search/itinerary.hpp"

namespace safar::journey {

struct Endpoint {
    enum class Type { Place, Station };
    Type type = Type::Place;
    std::size_t index = 0;  // into Dataset::places() or Dataset::stations()

    bool operator==(const Endpoint&) const = default;
};

struct ModeSet {
    bool walk = true;
    bool metro = true;
    bool cab = true;
    bool autorickshaw = true;
    bool bike = true;

    bool road(graph::RoadMode m) const;
    bool any_road() const { return cab || autorickshaw || bike; }
};

struct JourneyRequest {
    Endpoint from;
    Endpoint to;
    int hour = 9;         // departure hour 0-23 (drives the traffic factor)
    bool sunday = false;  // Sunday / national holiday metro fares
    ModeSet modes;
    double preference = 0.5;  // 0 = cheapest ... 1 = fastest
    std::size_t max_results = 5;
};

// The same road leg with each vehicle type, so the user can pick ("cab ₹240 / auto ₹180").
struct RoadOption {
    graph::RoadMode mode = graph::RoadMode::Cab;
    double total_min = 0;
    double wait_min = 0;
    double fare_rs = 0;
    double distance_km = 0;
};

struct Route {
    search::Itinerary itinerary;
    std::vector<std::vector<RoadOption>> road_options;  // one list per leg, empty unless it is a road leg
    std::vector<std::string> tags;  // fastest, cheapest, fewest_changes, most_expensive, recommended
    std::string found_by;           // which search produced it
    double score = 0;               // minutes + rupees / value_of_time (lower is better)
    bool dominated = false;         // shown only for reference (the direct cab when another route beats it)
};

struct JourneyResult {
    graph::Graph graph;  // the per-query graph the routes' node ids refer to
    graph::NodeId origin = 0;
    graph::NodeId destination = 0;
    std::vector<Route> routes;
    double value_of_time = 0;  // rupees per minute used for "recommended"
    std::size_t searches = 0;
    std::size_t candidates = 0;
    std::size_t distinct = 0;
    std::size_t pareto = 0;
};

class PlanError : public std::runtime_error {
public:
    PlanError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), status(status), code(std::move(code)) {}
    int status;
    std::string code;
};

// Multi-modal journey planner. Thread-safe: plan() only reads shared data and works
// on its own copy of the transit graph.
class Planner {
public:
    Planner(std::shared_ptr<const data::Dataset> ds, std::shared_ptr<const fares::TransportConfig> cfg,
            std::shared_ptr<const roads::RoadMatrix> roads, std::shared_ptr<const graph::Graph> transit);

    JourneyResult plan(const JourneyRequest& req) const;

private:
    struct Point {
        graph::NodeId node;
        geo::LatLng pos;
        std::string key;  // road matrix key
        std::size_t place = graph::kNone;  // place index for place endpoints
    };

    Point attach(graph::Graph& g, const Endpoint& e) const;
    Point station_point(const graph::Graph& g, std::size_t station) const;
    std::vector<std::size_t> access_stations(geo::LatLng pos) const;
    roads::RoadLeg road_leg(const Point& a, const Point& b) const;
    // access = true for a ride to/from a station (shorter limits), false for the whole trip.
    void add_links(graph::Graph& g, const Point& a, const Point& b, const JourneyRequest& req, bool access) const;

    std::shared_ptr<const data::Dataset> ds_;
    std::shared_ptr<const fares::TransportConfig> cfg_;
    std::shared_ptr<const roads::RoadMatrix> roads_;
    std::shared_ptr<const graph::Graph> transit_;
};

nlohmann::json to_json(const JourneyResult& r, const data::Dataset& ds);

}  // namespace safar::journey
