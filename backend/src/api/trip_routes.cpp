// Trip planner: the best order to visit several stops (local search on a small TSP).
//
// POST /api/v1/trips/optimize
// {
//   "stops": ["bennett-university", "place:india-gate", "station:hauz-khas", ...],
//                                  3-10 ids (or {type, id}); the first one is the start
//   "method": "both",              optional: hill_climb | simulated_annealing (sa) | both
//   "round_trip": false,           optional: come back to the start at the end
//   "depart_at": "2026-10-27T09:00", optional, local time; default now
//   "preference": 0.5,             optional, 0 = cheapest ... 1 = fastest
//   "seed": 42,                    optional, makes the random searches repeatable
//   "restarts": 8,                 optional, hill climbing climbs (1-50)
//   "iterations": 4000             optional, simulated annealing steps (100-50000)
// }
//
// Every pair of stops is planned with the journey planner (respecting line closures);
// the cost of a leg is the recommended route's score (minutes + rupees / value of time).
// With at most 10 stops the exact optimum is also found by trying every order, so each
// method reports its gap to it.

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "api/params.hpp"
#include "api/server.hpp"
#include "optimize/tour.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

constexpr std::size_t kMinStops = 3;
constexpr std::size_t kMaxStops = 10;

double round1(double x) { return std::round(x * 10.0) / 10.0; }
double round2(double x) { return std::round(x * 100.0) / 100.0; }

std::size_t optional_int(const json& body, const char* key, std::size_t fallback, std::size_t lo, std::size_t hi) {
    if (!body.contains(key)) return fallback;
    const auto& v = body[key];
    if (!v.is_number_integer() || v.get<long long>() < static_cast<long long>(lo) ||
        v.get<long long>() > static_cast<long long>(hi)) {
        throw BadRequest{400, "invalid_field",
                         std::string(key) + " must be an integer from " + std::to_string(lo) + " to " + std::to_string(hi)};
    }
    return v.get<std::size_t>();
}

// One leg of the matrix: the recommended route between two stops.
struct Pair {
    double score = 0;
    double time_min = 0;
    double fare_rs = 0;
    json route;
};

json order_json(const optimize::Order& o) {
    json a = json::array();
    for (auto i : o) a.push_back(i);
    return a;
}

}  // namespace

void Server::register_trip_routes() {
    http_.Post("/api/v1/trips/optimize", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const json body = json::parse(req.body);  // bad JSON -> 400 via the exception handler
        try {
            if (!body.is_object()) throw BadRequest{400, "invalid_body", "body must be a JSON object"};
            const auto& ds = *ctx_.dataset;

            // ---- request ----
            if (!body.contains("stops") || !body["stops"].is_array()) {
                throw BadRequest{400, "invalid_field", "stops must be an array of place or station ids"};
            }
            const auto& raw = body["stops"];
            if (raw.size() < kMinStops || raw.size() > kMaxStops) {
                throw BadRequest{400, "invalid_field", "stops must have 3 to 10 entries (the first is the start)"};
            }
            std::vector<journey::Endpoint> stops;
            for (const auto& s : raw) {
                const auto e = parse_endpoint(json{{"stop", s}}, "stop", ds);
                for (const auto& prev : stops) {
                    if (prev == e) throw BadRequest{400, "duplicate_stop", "each stop can appear only once"};
                }
                stops.push_back(e);
            }

            if (body.contains("method") && !body["method"].is_string()) {
                throw BadRequest{400, "unknown_method", "method must be hill_climb, simulated_annealing or both"};
            }
            const std::string method = body.value("method", std::string("both"));
            const bool run_hc = method == "hill_climb" || method == "both";
            const bool run_sa = method == "simulated_annealing" || method == "sa" || method == "both";
            if (!run_hc && !run_sa) {
                throw BadRequest{400, "unknown_method", "method must be hill_climb, simulated_annealing or both"};
            }
            if (body.contains("round_trip") && !body["round_trip"].is_boolean()) {
                throw BadRequest{400, "invalid_field", "round_trip must be true or false"};
            }
            const bool round_trip = body.value("round_trip", false);

            std::string at;
            if (body.contains("depart_at")) {
                if (!body["depart_at"].is_string()) {
                    throw BadRequest{400, "invalid_field", "depart_at must look like 2026-10-27T09:00"};
                }
                at = body["depart_at"].get<std::string>();
            }
            const auto depart = parse_depart(body.contains("depart_at") ? &at : nullptr, "depart_at");
            double preference = 0.5;
            if (body.contains("preference")) {
                const auto& p = body["preference"];
                if (!p.is_number() || p.get<double>() < 0 || p.get<double>() > 1) {
                    throw BadRequest{400, "invalid_field", "preference must be a number from 0 to 1"};
                }
                preference = p.get<double>();
            }
            const auto seed = optional_int(body, "seed", 42, 0, 2147483647);
            const auto restarts = optional_int(body, "restarts", 8, 1, 50);
            const auto iterations = optional_int(body, "iterations", 4000, 100, 50000);

            // ---- cost matrix: one journey plan per ordered pair of stops ----
            const auto closures = disruptions_.snapshot();
            const std::size_t n = stops.size();
            std::vector<std::vector<Pair>> pairs(n, std::vector<Pair>(n));
            optimize::TourProblem problem;
            problem.round_trip = round_trip;
            problem.cost.assign(n, std::vector<double>(n, 0.0));
            double vot = 0;
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = 0; j < n; ++j) {
                    if (i == j) continue;
                    journey::JourneyRequest jr;
                    jr.from = stops[i];
                    jr.to = stops[j];
                    jr.hour = depart.hour;
                    jr.sunday = depart.sunday;
                    jr.preference = preference;
                    jr.max_results = 1;  // keeps only the recommended route
                    jr.closures = closures.get();
                    journey::JourneyResult result;
                    try {
                        result = ctx_.planner->plan(jr);
                    } catch (const journey::PlanError& e) {
                        throw BadRequest{422, "unreachable_stop",
                                         "no route from " + endpoint_ref(ds, stops[i])["name"].get<std::string>() +
                                             " to " + endpoint_ref(ds, stops[j])["name"].get<std::string>() + ": " +
                                             e.what()};
                    }
                    vot = result.value_of_time;
                    const auto& route = result.routes.front();
                    auto& pr = pairs[i][j];
                    pr.score = route.score;
                    pr.time_min = route.itinerary.total_min;
                    pr.fare_rs = route.itinerary.fare_rs;
                    pr.route = std::move(journey::to_json(result, ds)["routes"][0]);
                    problem.cost[i][j] = pr.score;
                }
            }
            const double matrix_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

            // ---- search ----
            auto totals = [&](const optimize::Order& o) {
                double time = 0, fare = 0;
                auto add = [&](std::size_t a, std::size_t b) {
                    time += pairs[a][b].time_min;
                    fare += pairs[a][b].fare_rs;
                };
                for (std::size_t k = 0; k + 1 < o.size(); ++k) add(o[k], o[k + 1]);
                if (round_trip) add(o.back(), o.front());
                return json{{"order", order_json(o)},
                            {"score", round1(optimize::tour_cost(problem, o))},
                            {"total_min", round1(time)},
                            {"fare_rs", round1(fare)}};
            };

            const auto initial = optimize::identity_order(n);
            const auto optimum = optimize::exhaustive(problem, kMaxStops);

            struct Run {
                const char* name;
                optimize::LocalSearchResult r;
            };
            std::vector<Run> runs;
            if (run_hc) {
                optimize::HillClimbOptions o;
                o.restarts = restarts;
                o.seed = seed;
                runs.push_back({"hill_climb", optimize::hill_climb(problem, initial, o)});
            }
            if (run_sa) {
                optimize::AnnealOptions o;
                o.iterations = iterations;
                o.seed = seed;
                runs.push_back({"simulated_annealing", optimize::simulated_annealing(problem, initial, o)});
            }

            json results = json::array();
            std::size_t best = 0;
            for (std::size_t k = 0; k < runs.size(); ++k) {
                const auto& [name, r] = runs[k];
                if (r.cost < runs[best].r.cost - 1e-9) best = k;
                json j = totals(r.order);
                j["method"] = name;
                j["evaluations"] = r.evaluations;
                j["moves"] = r.moves;
                j["runtime_ms"] = round2(r.runtime_ms);
                if (std::string(name) == "hill_climb") j["restarts"] = r.restarts;
                else j["accepted_worse"] = r.accepted_worse;
                if (optimum && optimum->cost > 0) {
                    j["gap_pct"] = round2((r.cost - optimum->cost) / optimum->cost * 100.0);
                }
                json curve = json::array();
                for (const auto& pt : r.curve) {
                    json c = {{"evaluations", pt.evaluations}, {"current", round2(pt.current)}, {"best", round2(pt.best)}};
                    if (std::string(name) != "hill_climb") c["temperature"] = round2(pt.temperature);
                    curve.push_back(std::move(c));
                }
                j["curve"] = std::move(curve);
                results.push_back(std::move(j));
            }

            // The winning order, leg by leg, with each leg's full route for the map.
            const auto& win = runs[best];
            json legs = json::array();
            auto add_leg = [&](std::size_t a, std::size_t b) {
                legs.push_back({{"from", a}, {"to", b}, {"route", pairs[a][b].route}});
            };
            for (std::size_t k = 0; k + 1 < win.r.order.size(); ++k) add_leg(win.r.order[k], win.r.order[k + 1]);
            if (round_trip) add_leg(win.r.order.back(), win.r.order.front());
            json best_json = totals(win.r.order);
            best_json["method"] = win.name;
            best_json["legs"] = std::move(legs);

            json stops_json = json::array();
            for (const auto& s : stops) stops_json.push_back(endpoint_ref(ds, s));
            json matrix = {{"score", json::array()}, {"total_min", json::array()}, {"fare_rs", json::array()}};
            for (std::size_t i = 0; i < n; ++i) {
                json rs = json::array(), rt = json::array(), rf = json::array();
                for (std::size_t j = 0; j < n; ++j) {
                    rs.push_back(i == j ? json(nullptr) : json(round1(pairs[i][j].score)));
                    rt.push_back(i == j ? json(nullptr) : json(round1(pairs[i][j].time_min)));
                    rf.push_back(i == j ? json(nullptr) : json(round1(pairs[i][j].fare_rs)));
                }
                matrix["score"].push_back(std::move(rs));
                matrix["total_min"].push_back(std::move(rt));
                matrix["fare_rs"].push_back(std::move(rf));
            }

            json data = {
                {"stops", std::move(stops_json)},
                {"round_trip", round_trip},
                {"depart", {{"at", depart.at}, {"hour", depart.hour}, {"sunday_fares", depart.sunday}}},
                {"value_of_time_rs_per_min", round1(vot)},
                {"matrix", std::move(matrix)},
                {"matrix_ms", round1(matrix_ms)},
                {"input", totals(initial)},
                {"results", std::move(results)},
                {"best", std::move(best_json)},
                {"closures", closures->closures.size()},
                {"fare_note", fares::kEstimateNote},
            };
            if (optimum) {
                json o = totals(optimum->order);
                o["evaluations"] = optimum->evaluations;
                o["runtime_ms"] = round2(optimum->runtime_ms);
                data["optimum"] = std::move(o);
            } else {
                data["optimum"] = nullptr;
            }
            send_ok(res, t0, data);
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        }
    });
}

}  // namespace safar::api
