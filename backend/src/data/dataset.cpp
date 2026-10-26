#include "data/dataset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace safar::data {

using nlohmann::json;

namespace {

json read_json(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("cannot open " + file.string() + " (run the scripts in scripts/ to build it)");
    }
    return json::parse(in);
}

TransitMode parse_mode(const std::string& s) {
    if (s == "metro") return TransitMode::Metro;
    if (s == "airport_express") return TransitMode::AirportExpress;
    if (s == "rrts") return TransitMode::Rrts;
    throw std::runtime_error("unknown line mode: " + s);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// 3 = name starts with q, 2 = some word starts with q, 1 = substring, 0 = no match.
int match_score(const std::string& name_lc, const std::string& q) {
    const auto pos = name_lc.find(q);
    if (pos == std::string::npos) return 0;
    if (pos == 0) return 3;
    for (auto p = pos; p != std::string::npos; p = name_lc.find(q, p + 1)) {
        if (!std::isalnum(static_cast<unsigned char>(name_lc[p - 1]))) return 2;
    }
    return 1;
}

}  // namespace

Dataset Dataset::load(const std::filesystem::path& dir) {
    Dataset d;
    const json metro = read_json(dir / "metro.json");
    const json places = read_json(dir / "places.json");

    for (const auto& j : metro.at("lines")) {
        d.lines_.push_back({j.at("id"), j.at("name"), j.at("color"), j.at("operator"),
                            parse_mode(j.at("mode")), j.at("avg_wait_min"),
                            j.at("stations").get<std::vector<std::string>>()});
    }
    for (const auto& j : metro.at("stations")) {
        d.stations_.push_back({j.at("id"), j.at("name"), {j.at("lat"), j.at("lng")},
                               j.at("lines").get<std::vector<std::string>>()});
    }
    for (const auto& j : metro.at("segments")) {
        d.segments_.push_back({j.at("line"), j.at("a"), j.at("b"), j.at("distance_km"), j.at("time_min")});
    }
    for (const auto& j : metro.at("transfers")) {
        Transfer t;
        if (j.at("type") == "in_station") {
            t.kind = Transfer::Kind::InStation;
            t.station = j.at("station");
        } else {
            t.kind = Transfer::Kind::WalkLink;
            t.a = j.at("a");
            t.b = j.at("b");
            t.distance_km = j.at("distance_km");
        }
        t.walk_min = j.at("walk_min");
        d.transfers_.push_back(std::move(t));
    }
    for (const auto& j : places.at("places")) {
        d.places_.push_back({j.at("id"), j.at("name"), j.at("category"), j.at("city"),
                             {j.at("lat"), j.at("lng")},
                             j.at("nearest_station").at("id"), j.at("nearest_station").at("distance_km")});
    }

    d.build_indexes();
    return d;
}

void Dataset::build_indexes() {
    auto index = [](auto& vec, auto& map, const char* what) {
        for (std::size_t i = 0; i < vec.size(); ++i) {
            if (!map.emplace(vec[i].id, i).second) {
                throw std::runtime_error(std::string("duplicate ") + what + " id: " + vec[i].id);
            }
        }
    };
    index(lines_, line_idx_, "line");
    index(stations_, station_idx_, "station");
    index(places_, place_idx_, "place");

    // Referential integrity: a typo in the data should fail at startup, not mid-request.
    auto need_station = [this](const std::string& id, const std::string& where) {
        if (!station(id)) throw std::runtime_error("unknown station '" + id + "' in " + where);
    };
    for (const auto& l : lines_) {
        for (const auto& s : l.stations) need_station(s, "line " + l.id);
    }
    for (const auto& s : segments_) {
        if (!line(s.line)) throw std::runtime_error("unknown line '" + s.line + "' in segment");
        need_station(s.a, "segment");
        need_station(s.b, "segment");
    }
    for (const auto& t : transfers_) {
        if (t.kind == Transfer::Kind::InStation) {
            need_station(t.station, "transfer");
        } else {
            need_station(t.a, "walk link");
            need_station(t.b, "walk link");
        }
    }
    for (const auto& p : places_) need_station(p.nearest_station, "place " + p.id);

    station_lc_.clear();
    place_lc_.clear();
    for (const auto& s : stations_) station_lc_.push_back(lower(s.name));
    for (const auto& p : places_) place_lc_.push_back(lower(p.name));
}

const Line* Dataset::line(const std::string& id) const {
    auto it = line_idx_.find(id);
    return it == line_idx_.end() ? nullptr : &lines_[it->second];
}

const Station* Dataset::station(const std::string& id) const {
    auto it = station_idx_.find(id);
    return it == station_idx_.end() ? nullptr : &stations_[it->second];
}

const Place* Dataset::place(const std::string& id) const {
    auto it = place_idx_.find(id);
    return it == place_idx_.end() ? nullptr : &places_[it->second];
}

std::vector<SearchHit> Dataset::search(const std::string& query, std::size_t limit) const {
    const std::string q = lower(query);
    std::vector<SearchHit> hits;
    if (q.empty()) return hits;

    for (std::size_t i = 0; i < places_.size(); ++i) {
        // Places rank slightly above stations with the same match quality.
        if (int s = match_score(place_lc_[i], q)) hits.push_back({SearchHit::Kind::Place, &places_[i], nullptr, s * 2 + 1});
    }
    for (std::size_t i = 0; i < stations_.size(); ++i) {
        if (int s = match_score(station_lc_[i], q)) hits.push_back({SearchHit::Kind::Station, nullptr, &stations_[i], s * 2});
    }

    auto name = [](const SearchHit& h) -> const std::string& { return h.place ? h.place->name : h.station->name; };
    std::stable_sort(hits.begin(), hits.end(), [&](const SearchHit& x, const SearchHit& y) {
        if (x.score != y.score) return x.score > y.score;
        if (name(x).size() != name(y).size()) return name(x).size() < name(y).size();
        return name(x) < name(y);
    });
    if (hits.size() > limit) hits.resize(limit);
    return hits;
}

const char* to_string(TransitMode m) {
    switch (m) {
        case TransitMode::Metro: return "metro";
        case TransitMode::AirportExpress: return "airport_express";
        case TransitMode::Rrts: return "rrts";
    }
    return "metro";
}

json to_json(const Line& l) {
    return {{"id", l.id}, {"name", l.name}, {"color", l.color}, {"operator", l.operator_code},
            {"mode", to_string(l.mode)}, {"avg_wait_min", l.avg_wait_min}, {"stations", l.stations}};
}

json to_json(const Station& s) {
    return {{"id", s.id}, {"name", s.name}, {"lat", s.pos.lat}, {"lng", s.pos.lng}, {"lines", s.lines}};
}

json to_json(const Place& p) {
    return {{"id", p.id}, {"name", p.name}, {"category", p.category}, {"city", p.city},
            {"lat", p.pos.lat}, {"lng", p.pos.lng},
            {"nearest_station", {{"id", p.nearest_station}, {"distance_km", p.nearest_station_km}}}};
}

}  // namespace safar::data
