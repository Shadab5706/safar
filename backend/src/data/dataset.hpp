#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "util/geo.hpp"

namespace safar::data {

enum class TransitMode { Metro, AirportExpress, Rrts };

struct Line {
    std::string id;
    std::string name;
    std::string color;
    std::string operator_code;  // DMRC, NMRC, RAPID, NCRTC: picks the fare table
    TransitMode mode = TransitMode::Metro;
    double avg_wait_min = 0;
    std::vector<std::string> stations;  // in running order (one direction)
};

struct Station {
    std::string id;
    std::string name;
    geo::LatLng pos;
    std::vector<std::string> lines;
};

// Ride between two neighbouring stations on one line. Usable in both directions.
struct Segment {
    std::string line;
    std::string a, b;
    double distance_km = 0;
    double time_min = 0;
};

// Either a change of line inside one station (station set, a/b empty) or a short
// walk between two different stations (a/b set).
struct Transfer {
    enum class Kind { InStation, WalkLink } kind = Kind::InStation;
    std::string station;
    std::string a, b;
    double distance_km = 0;
    double walk_min = 0;
};

struct Place {
    std::string id;
    std::string name;
    std::string category;
    std::string city;
    geo::LatLng pos;
    std::string nearest_station;
    double nearest_station_km = 0;
};

// One autocomplete hit: a place or a metro station.
struct SearchHit {
    enum class Kind { Place, Station } kind;
    const Place* place = nullptr;
    const Station* station = nullptr;
    int score = 0;
};

// Read-only snapshot of all static data, loaded once at startup and shared by
// every request thread.
class Dataset {
public:
    // Loads metro.json + places.json from `dir`. Throws std::runtime_error with a
    // clear message if a file is missing or references an unknown id.
    static Dataset load(const std::filesystem::path& dir);

    const std::vector<Line>& lines() const { return lines_; }
    const std::vector<Station>& stations() const { return stations_; }
    const std::vector<Segment>& segments() const { return segments_; }
    const std::vector<Transfer>& transfers() const { return transfers_; }
    const std::vector<Place>& places() const { return places_; }

    const Line* line(const std::string& id) const;
    const Station* station(const std::string& id) const;
    const Place* place(const std::string& id) const;

    // Case-insensitive autocomplete over place and station names.
    // Ranking: whole name starts with query > a word starts with query > substring.
    std::vector<SearchHit> search(const std::string& query, std::size_t limit = 10) const;

private:
    void build_indexes();

    std::vector<Line> lines_;
    std::vector<Station> stations_;
    std::vector<Segment> segments_;
    std::vector<Transfer> transfers_;
    std::vector<Place> places_;

    std::unordered_map<std::string, std::size_t> line_idx_, station_idx_, place_idx_;
    std::vector<std::string> station_lc_, place_lc_;  // lower-cased names for search
};

const char* to_string(TransitMode m);
nlohmann::json to_json(const Line& l);
nlohmann::json to_json(const Station& s);
nlohmann::json to_json(const Place& p);

}  // namespace safar::data
