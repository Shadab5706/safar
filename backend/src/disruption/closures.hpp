#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "data/dataset.hpp"
#include "graph/graph.hpp"

namespace safar::disruption {

// A closed station or a closed section of one line.
//
// Station: trains run through without stopping, as DMRC does when it shuts a station.
//          Nobody can board, alight or change lines there; the street outside still works.
// Segment: no trains between two neighbouring stations of one line, in either direction.
//          This is what cuts a line in two.
struct Closure {
    enum class Type { Station, Segment };
    Type type = Type::Station;
    std::string station;  // Station
    std::string line;     // Segment
    std::string a, b;     // Segment ends, a < b so both directions give the same closure

    // "station:<id>" or "segment:<line>:<a>:<b>"
    std::string id() const;
    bool operator==(const Closure&) const = default;
};

class ClosureError : public std::runtime_error {
public:
    ClosureError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), status(status), code(std::move(code)) {}
    int status;
    std::string code;
};

// Validated constructors: the station must exist; a segment's stations must be
// neighbours on that line (there is a train between them). Throw ClosureError.
Closure station_closure(const data::Dataset& ds, const std::string& station);
Closure segment_closure(const data::Dataset& ds, const graph::Graph& transit, const std::string& line,
                        const std::string& a, const std::string& b);

// Edges of the transit graph the closures block (1 = blocked), size = edge_count().
// Edge ids of a per-query copy of the graph match, so the mask applies there too.
std::vector<char> blocked_edges(const graph::Graph& transit, const std::vector<Closure>& closures);

// One consistent view of the current closures, safe to read from any thread.
struct Snapshot {
    std::vector<Closure> closures;      // in the order they were added
    std::vector<char> blocked_edges;    // see blocked_edges()
    std::vector<char> closed_stations;  // index = Dataset::stations(), 1 = closed
    std::uint64_t version = 0;          // bumps on every change

    bool empty() const { return closures.empty(); }
    bool station_closed(std::size_t station) const {
        return station < closed_stations.size() && closed_stations[station];
    }
};

// Server-wide list of closures. Writers rebuild the snapshot; readers take a
// shared_ptr to the current one, so a request never sees a half-applied change.
class Store {
public:
    static constexpr std::size_t kMaxClosures = 50;

    Store(std::shared_ptr<const data::Dataset> ds, std::shared_ptr<const graph::Graph> transit);

    std::shared_ptr<const Snapshot> snapshot() const;
    // false if it was already closed. Throws ClosureError when the list is full.
    bool add(const Closure& c);
    // false if no closure has this id.
    bool remove(const std::string& id);
    void clear();

private:
    void publish(std::vector<Closure> closures);  // caller holds mu_

    std::shared_ptr<const data::Dataset> ds_;
    std::shared_ptr<const graph::Graph> transit_;
    mutable std::mutex mu_;
    std::shared_ptr<const Snapshot> current_;
};

// {"id", "type", "label", "station": {id, name, lat, lng}} or
// {"id", "type", "label", "line": {id, name, color}, "from": {...}, "to": {...}}
nlohmann::json to_json(const Closure& c, const data::Dataset& ds);
nlohmann::json to_json(const Snapshot& s, const data::Dataset& ds);

}  // namespace safar::disruption
