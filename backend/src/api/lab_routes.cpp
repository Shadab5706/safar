// Algorithm Lab: run several search algorithms on the same query and compare them.
//
// POST /api/v1/lab/compare
// {
//   "from": "rajiv-chowk", "to": "hauz-khas",       station ids (required)
//   "algorithms": ["bfs","dfs","ucs","greedy","astar"],  optional, default all
//   "include_expansion": true,                      optional, node ids in expansion order
//   "heuristic_speed_kmh": 110,                     optional, lower = inadmissible heuristic
//   "boarding_penalty_min": 0                       optional, extra minutes per train boarded
// }
// Every algorithm skips the edges the current line closures block.

#include <cmath>
#include <string>
#include <vector>

#include "api/server.hpp"
#include "search/itinerary.hpp"
#include "search/search.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

double round2(double x) { return std::round(x * 100.0) / 100.0; }

// Small validation error carrying an HTTP status + machine-readable code.
struct BadRequest {
    int status;
    std::string code;
    std::string message;
};

double optional_number(const json& body, const char* key, double fallback, double lo, double hi) {
    if (!body.contains(key)) return fallback;
    const auto& v = body[key];
    if (!v.is_number()) throw BadRequest{400, "invalid_field", std::string(key) + " must be a number"};
    const double x = v.get<double>();
    if (x < lo || x > hi) {
        throw BadRequest{400, "invalid_field",
                         std::string(key) + " must be between " + std::to_string(lo) + " and " + std::to_string(hi)};
    }
    return x;
}

graph::NodeId station_node(const graph::Graph& g, const data::Dataset& ds, const json& body, const char* key) {
    if (!body.contains(key) || !body[key].is_string()) {
        throw BadRequest{400, "invalid_field", std::string(key) + " must be a station id string"};
    }
    const auto id = body[key].get<std::string>();
    const auto node = g.find(id);
    if (!node || !ds.station(id)) throw BadRequest{404, "station_not_found", "no station with id " + id};
    return *node;
}

}  // namespace

void Server::register_lab_routes() {
    http_.Post("/api/v1/lab/compare", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const json body = json::parse(req.body);  // bad JSON -> 400 via the exception handler
        try {
            if (!body.is_object()) throw BadRequest{400, "invalid_body", "body must be a JSON object"};
            const auto& g = *ctx_.transit;
            const auto start = station_node(g, *ctx_.dataset, body, "from");
            const auto goal = station_node(g, *ctx_.dataset, body, "to");

            std::vector<search::Algorithm> algos;
            if (body.contains("algorithms")) {
                if (!body["algorithms"].is_array() || body["algorithms"].empty()) {
                    throw BadRequest{400, "invalid_field", "algorithms must be a non-empty array"};
                }
                for (const auto& a : body["algorithms"]) {
                    const auto parsed = a.is_string() ? search::parse_algorithm(a.get<std::string>()) : std::nullopt;
                    if (!parsed) throw BadRequest{400, "unknown_algorithm", "use bfs, dfs, ucs, greedy or astar"};
                    algos.push_back(*parsed);
                }
            } else {
                algos.assign(std::begin(search::kAllAlgorithms), std::end(search::kAllAlgorithms));
            }

            search::SearchOptions opt;
            opt.heuristic_speed_kmh = optional_number(body, "heuristic_speed_kmh", 0, 1, 1000);
            opt.cost.boarding_penalty = optional_number(body, "boarding_penalty_min", 0, 0, 120);
            opt.record_expansion = body.value("include_expansion", true);
            const auto closures = disruptions_.snapshot();
            if (!closures->empty()) opt.blocked_edges = &closures->blocked_edges;

            // UCS is always run as the reference for the optimal cost.
            search::SearchOptions ref_opt = opt;
            ref_opt.record_expansion = false;
            const auto reference = search::uniform_cost(g, start, goal, ref_opt);

            json results = json::array();
            for (auto algo : algos) {
                const auto r = search::run(algo, g, start, goal, opt);
                json j = {
                    {"algorithm", search::to_string(algo)},
                    {"found", r.found},
                    {"cost", round2(r.cost)},
                    {"stats", {{"nodes_expanded", r.stats.nodes_expanded},
                               {"nodes_generated", r.stats.nodes_generated},
                               {"max_frontier", r.stats.max_frontier},
                               {"runtime_us", round2(r.stats.runtime_us)}}},
                };
                if (r.found && reference.found && reference.cost > 0) {
                    j["optimality_gap_pct"] = round2((r.cost - reference.cost) / reference.cost * 100.0);
                }
                if (r.found) j["itinerary"] = search::to_json(search::build_itinerary(g, r.path), g, *ctx_.dataset);
                if (opt.record_expansion) {
                    json order = json::array();
                    for (auto n : r.expansion_order) order.push_back(g.node(n).id);
                    j["expansion"] = std::move(order);
                }
                results.push_back(std::move(j));
            }

            const auto& from = *ctx_.dataset->station(g.node(start).id);
            const auto& to = *ctx_.dataset->station(g.node(goal).id);
            send_ok(res, t0, {
                {"from", {{"id", from.id}, {"name", from.name}}},
                {"to", {{"id", to.id}, {"name", to.name}}},
                {"optimal_cost", reference.found ? json(round2(reference.cost)) : json(nullptr)},
                {"heuristic_speed_kmh", round2(search::Heuristic(g, goal, opt).speed_kmh())},
                {"closures", closures->closures.size()},
                {"results", std::move(results)},
            });
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        }
    });
}

}  // namespace safar::api
