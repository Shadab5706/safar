// Journey planning.
//
// POST /api/v1/journeys
// {
//   "from": {"type": "place", "id": "bennett-university"},   or "bennett-university",
//   "to":   "station:hauz-khas",                               "place:<id>", "station:<id>"
//   "depart_at": "2026-10-27T09:00",     optional, local time; default now
//   "modes": ["walk","metro","cab","auto","bike"],   optional, default all
//   "preference": 0.5,                   optional, 0 = cheapest ... 1 = fastest
//   "max_results": 5                     optional, 1-8
// }
// Routes avoid the current line closures (GET /api/v1/disruptions).

#include <string>

#include "api/params.hpp"
#include "api/server.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

journey::ModeSet parse_modes(const json& body) {
    if (!body.contains("modes")) return {};
    const auto& v = body["modes"];
    if (!v.is_array() || v.empty()) throw BadRequest{400, "invalid_field", "modes must be a non-empty array"};
    journey::ModeSet m{false, false, false, false, false};
    for (const auto& x : v) {
        const auto s = x.is_string() ? x.get<std::string>() : std::string();
        if (s == "walk") m.walk = true;
        else if (s == "metro") m.metro = true;
        else if (s == "cab") m.cab = true;
        else if (s == "auto") m.autorickshaw = true;
        else if (s == "bike") m.bike = true;
        else throw BadRequest{400, "unknown_mode", "modes can be walk, metro, cab, auto, bike"};
    }
    return m;
}

}  // namespace

void Server::register_journey_routes() {
    http_.Post("/api/v1/journeys", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const json body = json::parse(req.body);  // bad JSON -> 400 via the exception handler
        try {
            if (!body.is_object()) throw BadRequest{400, "invalid_body", "body must be a JSON object"};
            const auto& ds = *ctx_.dataset;

            journey::JourneyRequest jr;
            jr.from = parse_endpoint(body, "from", ds);
            jr.to = parse_endpoint(body, "to", ds);
            std::string at;
            if (body.contains("depart_at")) {
                if (!body["depart_at"].is_string()) {
                    throw BadRequest{400, "invalid_field", "depart_at must look like 2026-10-27T09:00"};
                }
                at = body["depart_at"].get<std::string>();
            }
            const auto depart = parse_depart(body.contains("depart_at") ? &at : nullptr, "depart_at");
            jr.hour = depart.hour;
            jr.sunday = depart.sunday;
            jr.modes = parse_modes(body);

            if (body.contains("preference")) {
                const auto& p = body["preference"];
                if (!p.is_number() || p.get<double>() < 0 || p.get<double>() > 1) {
                    throw BadRequest{400, "invalid_field", "preference must be a number from 0 to 1"};
                }
                jr.preference = p.get<double>();
            }
            if (body.contains("max_results")) {
                const auto& m = body["max_results"];
                if (!m.is_number_integer() || m.get<int>() < 1 || m.get<int>() > 8) {
                    throw BadRequest{400, "invalid_field", "max_results must be an integer from 1 to 8"};
                }
                jr.max_results = m.get<std::size_t>();
            }

            const auto closures = disruptions_.snapshot();
            jr.closures = closures.get();
            const auto result = ctx_.planner->plan(jr);
            json data = journey::to_json(result, ds);
            data["closures"] = closures->closures.size();
            data["depart"] = {{"at", depart.at}, {"hour", jr.hour}, {"sunday_fares", jr.sunday}};
            data["fare_note"] = fares::kEstimateNote;
            send_ok(res, t0, data);
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        } catch (const journey::PlanError& e) {
            send_error(res, e.status, e.code, e.what());
        }
    });
}

}  // namespace safar::api
