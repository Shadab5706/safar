#include "fares/transport_config.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace safar::fares {

using nlohmann::json;

namespace {

std::vector<std::pair<double, double>> parse_slabs(const json& arr, const std::string& where) {
    std::vector<std::pair<double, double>> out;
    for (const auto& s : arr) {
        const double up_to = s.at(0).is_null() ? std::numeric_limits<double>::infinity() : s.at(0).get<double>();
        out.emplace_back(up_to, s.at(1).get<double>());
    }
    if (out.empty() || !std::isinf(out.back().first)) {
        throw std::runtime_error(where + ": the last fare slab must have null (no upper limit)");
    }
    for (std::size_t i = 1; i < out.size(); ++i) {
        if (out[i].first <= out[i - 1].first || out[i].second < out[i - 1].second) {
            throw std::runtime_error(where + ": slabs must increase in distance and never decrease in fare");
        }
    }
    return out;
}

double slab_fare(const std::vector<std::pair<double, double>>& slabs, double km) {
    for (const auto& [up_to, fare] : slabs) {
        if (km <= up_to) return fare;
    }
    return slabs.back().second;
}

}  // namespace

double FareTable::fare(double km, bool on_sunday) const {
    return slab_fare(on_sunday && !sunday.empty() ? sunday : weekday, km);
}

double FareTable::base_fare() const { return weekday.front().second; }

double FareTable::per_km() const {
    if (weekday.size() < 2) return 0;
    const double last_km = weekday[weekday.size() - 2].first;  // last finite limit
    return (weekday.back().second - weekday.front().second) / last_km;
}

TransportConfig TransportConfig::load(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot open transport config " + file.string());
    const json j = json::parse(in, nullptr, true, /*ignore_comments=*/true);

    TransportConfig c;
    for (const auto& [zone, t] : j.at("metro_fares").items()) {
        FareTable ft;
        ft.verified = t.value("verified", false);
        ft.weekday = parse_slabs(t.at("weekday"), "metro_fares." + zone + ".weekday");
        if (t.contains("sunday")) ft.sunday = parse_slabs(t.at("sunday"), "metro_fares." + zone + ".sunday");
        c.metro.emplace(zone, std::move(ft));
    }

    for (auto mode : graph::kAllRoadModes) {
        const auto& r = j.at("road_modes").at(graph::to_string(mode));
        auto& p = c.road[static_cast<std::size_t>(mode)];
        p.base = r.at("base");
        p.base_km = r.at("base_km");
        p.per_km = r.at("per_km");
        p.per_min = r.at("per_min");
        p.min_fare = r.at("min_fare");
        p.pickup_min = r.at("pickup_min");
        p.speed_factor = r.at("speed_factor");
        p.max_km = r.at("max_km");
        p.max_access_km = r.at("max_access_km");
        if (p.pickup_min <= 0 || p.speed_factor <= 0 || p.max_km <= 0 || p.max_access_km <= 0) {
            throw std::runtime_error(std::string("road_modes.") + graph::to_string(mode) + ": invalid values");
        }
    }

    const auto& w = j.at("walk");
    c.walk = {w.at("speed_kmh"), w.at("detour_factor"), w.at("max_access_km"), w.at("max_direct_km"), {}};
    if (w.contains("max_km_by_category")) {
        for (const auto& [category, km] : w.at("max_km_by_category").items()) c.walk.max_km_by_category[category] = km;
    }
    c.fallback_detour = j.at("road_fallback").at("detour_factor");
    c.fallback_speed_kmh = j.at("road_fallback").at("speed_kmh");
    c.nearest_stations = j.at("access").at("nearest_stations");
    c.line_radius_km = j.at("access").at("line_radius_km");

    const auto& traffic = j.at("traffic_by_hour");
    if (traffic.size() != 24) throw std::runtime_error("traffic_by_hour needs 24 values");
    for (std::size_t h = 0; h < 24; ++h) {
        c.traffic[h] = traffic[h];
        if (c.traffic[h] < 1.0) throw std::runtime_error("traffic factors must be >= 1 (OSRM times are free-flow)");
    }

    c.vot_min = j.at("value_of_time").at("min");
    c.vot_max = j.at("value_of_time").at("max");
    if (c.vot_min <= 0 || c.vot_max < c.vot_min) throw std::runtime_error("value_of_time: need 0 < min <= max");
    return c;
}

const FareTable& TransportConfig::fare_table(const std::string& zone) const {
    auto it = metro.find(zone);
    if (it == metro.end()) throw std::runtime_error("no metro fare table for zone " + zone);
    return it->second;
}

double TransportConfig::road_fare(graph::RoadMode m, double km, double ride_min) const {
    const auto& p = road_params(m);
    const double fare = p.base + p.per_km * std::max(0.0, km - p.base_km) + p.per_min * ride_min;
    return std::max(p.min_fare, fare);
}

double TransportConfig::value_of_time(double preference) const {
    const double t = std::clamp(preference, 0.0, 1.0);
    return vot_min * std::pow(vot_max / vot_min, t);
}

std::string fare_zone(const data::Line& line) {
    if (line.mode == data::TransitMode::AirportExpress) return line.operator_code + "-AE";
    return line.operator_code;
}

}  // namespace safar::fares
