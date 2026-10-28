// Line closures ("disruptions"): close a station or a section of a line, and every
// journey, Algorithm Lab run and trip plan routes around it until it is reopened.
//
// GET    /api/v1/disruptions          -> { "items": [...], "version": n }
// POST   /api/v1/disruptions          close something:
//          { "type": "station", "station": "rajiv-chowk" }
//          { "type": "segment", "line": "yellow", "from": "rajiv-chowk", "to": "patel-chowk" }
//        -> 201 { "closure": {...}, "added": true, "items": [...], "version": n }
//           (200 with "added": false if it was already closed)
// DELETE /api/v1/disruptions/<id>     reopen one ("station:<id>" or "segment:<line>:<a>:<b>")
// DELETE /api/v1/disruptions          reopen everything

#include <string>

#include "api/server.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

std::string required_string(const json& body, const char* key) {
    if (!body.contains(key) || !body[key].is_string() || body[key].get<std::string>().empty()) {
        throw disruption::ClosureError(400, "invalid_field", std::string(key) + " must be a non-empty string");
    }
    return body[key].get<std::string>();
}

}  // namespace

void Server::register_disruption_routes() {
    http_.Get("/api/v1/disruptions", [this](const httplib::Request&, httplib::Response& res) {
        const auto t0 = Clock::now();
        send_ok(res, t0, disruption::to_json(*disruptions_.snapshot(), *ctx_.dataset));
    });

    http_.Post("/api/v1/disruptions", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const json body = json::parse(req.body);  // bad JSON -> 400 via the exception handler
        try {
            if (!body.is_object()) throw disruption::ClosureError(400, "invalid_body", "body must be a JSON object");
            const auto type = required_string(body, "type");
            disruption::Closure c;
            if (type == "station") {
                c = disruption::station_closure(*ctx_.dataset, required_string(body, "station"));
            } else if (type == "segment") {
                c = disruption::segment_closure(*ctx_.dataset, *ctx_.transit, required_string(body, "line"),
                                                required_string(body, "from"), required_string(body, "to"));
            } else {
                throw disruption::ClosureError(400, "invalid_field", "type must be station or segment");
            }
            const bool added = disruptions_.add(c);
            json data = disruption::to_json(*disruptions_.snapshot(), *ctx_.dataset);
            data["closure"] = disruption::to_json(c, *ctx_.dataset);
            data["added"] = added;
            send_ok(res, t0, data);
            if (added) res.status = 201;
        } catch (const disruption::ClosureError& e) {
            send_error(res, e.status, e.code, e.what());
        }
    });

    http_.Delete(R"(/api/v1/disruptions/([A-Za-z0-9:_\-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const std::string id = req.matches[1];
        if (!disruptions_.remove(id)) {
            send_error(res, 404, "closure_not_found", "nothing is closed with id " + id);
            return;
        }
        send_ok(res, t0, disruption::to_json(*disruptions_.snapshot(), *ctx_.dataset));
    });

    http_.Delete("/api/v1/disruptions", [this](const httplib::Request&, httplib::Response& res) {
        const auto t0 = Clock::now();
        disruptions_.clear();
        send_ok(res, t0, disruption::to_json(*disruptions_.snapshot(), *ctx_.dataset));
    });
}

}  // namespace safar::api
