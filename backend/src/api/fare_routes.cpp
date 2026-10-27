// Fare quotes for one road trip.
//
// GET /api/v1/fares/estimate?from=<id>&to=<id>&at=2026-10-27T09:00
//   from, to: "bennett-university", "place:<id>" or "station:<id>"
//   at:       optional local departure time (drives traffic, surge, night charges); default now
//
// Returns every provider's quote (Uber, Ola, Rapido, the Delhi auto meter), cheapest
// first. All of them are estimates; each carries a deep link to check the live price.

#include <cmath>

#include "api/params.hpp"
#include "api/server.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

double round1(double x) { return std::round(x * 10.0) / 10.0; }

}  // namespace

void Server::register_fare_routes() {
    http_.Get("/api/v1/fares/estimate", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        try {
            const auto& ds = *ctx_.dataset;
            for (const char* key : {"from", "to"}) {
                if (req.get_param_value(key).empty()) {
                    throw BadRequest{400, "invalid_field", std::string(key) + " is required"};
                }
            }
            const auto from = parse_endpoint_id(ds, req.get_param_value("from"));
            const auto to = parse_endpoint_id(ds, req.get_param_value("to"));
            if (from == to) throw BadRequest{400, "same_endpoints", "from and to are the same"};
            const std::string at = req.get_param_value("at");
            const auto depart = parse_depart(req.has_param("at") ? &at : nullptr, "at");

            const auto trip = ctx_.planner->road_trip(from, to, depart.hour);
            json quotes = json::array();
            for (const auto& q : ctx_.fares->quotes(trip)) quotes.push_back(fares::to_json(q));
            if (quotes.empty()) {
                throw BadRequest{422, "too_far", "no cab, auto or bike is offered for a trip this long"};
            }

            send_ok(res, t0,
                    {{"from", endpoint_ref(ds, from)},
                     {"to", endpoint_ref(ds, to)},
                     {"depart", {{"at", depart.at}, {"hour", depart.hour}}},
                     {"trip",
                      {{"distance_km", round1(trip.km)},
                       {"free_flow_min", round1(trip.free_flow_min)},
                       {"traffic_factor", ctx_.transport->traffic[static_cast<std::size_t>(depart.hour)]},
                       {"road_data", trip.from_osrm ? "osrm" : "straight_line_estimate"}}},
                     {"quotes", std::move(quotes)},
                     {"note", fares::kEstimateNote}});
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        }
    });
}

}  // namespace safar::api
