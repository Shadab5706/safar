#include "api/params.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace safar::api {

using nlohmann::json;

namespace {

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

}  // namespace

journey::Endpoint parse_endpoint_id(const data::Dataset& ds, const std::string& s) {
    if (s.rfind("place:", 0) == 0) return find_place(ds, s.substr(6));
    if (s.rfind("station:", 0) == 0) return find_station(ds, s.substr(8));
    if (ds.place(s)) return find_place(ds, s);
    if (ds.station(s)) return find_station(ds, s);
    throw BadRequest{404, "not_found", "no place or station with id " + s};
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
    return parse_endpoint_id(ds, v.get<std::string>());
}

DepartTime parse_depart(const std::string* iso, const char* field) {
    using namespace std::chrono;
    if (!iso) {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        char buf[20];
        std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M", &tm);
        return {buf, tm.tm_hour, tm.tm_wday == 0};
    }
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    if (std::sscanf(iso->c_str(), "%4d-%2d-%2dT%2d:%2d", &y, &mo, &d, &h, &mi) != 5) {
        throw BadRequest{400, "invalid_field", std::string(field) + " must look like 2026-10-27T09:00"};
    }
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || h < 0 || h > 23 || mi < 0 || mi > 59) {
        throw BadRequest{400, "invalid_field", std::string(field) + " is not a real date/time"};
    }
    return {iso->substr(0, 16), h, weekday{sys_days{ymd}} == Sunday};
}

json endpoint_ref(const data::Dataset& ds, const journey::Endpoint& e) {
    if (e.type == journey::Endpoint::Type::Station) {
        const auto& s = ds.stations()[e.index];
        return {{"type", "station"}, {"id", s.id}, {"name", s.name}, {"lat", s.pos.lat}, {"lng", s.pos.lng}};
    }
    const auto& p = ds.places()[e.index];
    return {{"type", "place"}, {"id", p.id}, {"name", p.name}, {"lat", p.pos.lat}, {"lng", p.pos.lng}};
}

}  // namespace safar::api
