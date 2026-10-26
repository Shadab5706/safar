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

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "api/server.hpp"

namespace safar::api {

using nlohmann::json;

namespace {

struct BadRequest {
    int status;
    std::string code;
    std::string message;
};

journey::Endpoint find_place(const data::Dataset& ds, const std::string& id) {
    const auto* p = ds.place(id);
    if (!p) throw BadRequest{404, "place_not_found", "no place with id " + id};
    return {journey::Endpoint::Type::Place, static_cast<std::size_t>(p - ds.places().data())};
}

journey::Endpoint find_station(const data::Dataset& ds, const std::string& id) {
    const auto* s = ds.station(id);
    if (!s) throw BadRequest{404, "station_not_found", "no station with id " + id};
    return {journey::Endpoint::Type::Station, static_cast<std::size_t>(s - ds.stations().data())};
}

journey::Endpoint parse_endpoint(const json& body, const char* key, const data::Dataset& ds) {
    if (!body.contains(key)) throw BadRequest{400, "invalid_field", std::string(key) + " is required"};
    const auto& v = body[key];
    if (v.is_object()) {
        const auto type = v.value("type", std::string());
        const auto id = v.value("id", std::string());
        if (type == "place") return find_place(ds, id);
        if (type == "station") return find_station(ds, id);
        throw BadRequest{400, "invalid_field", std::string(key) + ".type must be place or station"};
    }
    if (!v.is_string()) throw BadRequest{400, "invalid_field", std::string(key) + " must be an id or {type, id}"};
    const auto s = v.get<std::string>();
    if (s.rfind("place:", 0) == 0) return find_place(ds, s.substr(6));
    if (s.rfind("station:", 0) == 0) return find_station(ds, s.substr(8));
    if (ds.place(s)) return find_place(ds, s);
    if (ds.station(s)) return find_station(ds, s);
    throw BadRequest{404, "not_found", "no place or station with id " + s};
}

// Fills hour + sunday from "YYYY-MM-DDTHH:MM" (local time), or from the clock.
std::string parse_depart(const json& body, journey::JourneyRequest& req) {
    using namespace std::chrono;
    if (!body.contains("depart_at")) {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        req.hour = tm.tm_hour;
        req.sunday = tm.tm_wday == 0;
        char buf[20];
        std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M", &tm);
        return buf;
    }
    const auto& v = body["depart_at"];
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    if (!v.is_string() || std::sscanf(v.get<std::string>().c_str(), "%4d-%2d-%2dT%2d:%2d", &y, &mo, &d, &h, &mi) != 5) {
        throw BadRequest{400, "invalid_field", "depart_at must look like 2026-10-27T09:00"};
    }
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || h < 0 || h > 23 || mi < 0 || mi > 59) {
        throw BadRequest{400, "invalid_field", "depart_at is not a real date/time"};
    }
    req.hour = h;
    req.sunday = weekday{sys_days{ymd}} == Sunday;
    return v.get<std::string>().substr(0, 16);
}

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
            const std::string depart = parse_depart(body, jr);
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

            const auto result = ctx_.planner->plan(jr);
            json data = journey::to_json(result, ds);
            data["depart"] = {{"at", depart}, {"hour", jr.hour}, {"sunday_fares", jr.sunday}};
            send_ok(res, t0, data);
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        } catch (const journey::PlanError& e) {
            send_error(res, e.status, e.code, e.what());
        }
    });
}

}  // namespace safar::api
