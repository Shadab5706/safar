#include "disruption/closures.hpp"

#include <algorithm>

namespace safar::disruption {

using graph::EdgeKind;
using nlohmann::json;

namespace {

std::size_t station_index(const data::Dataset& ds, const std::string& id) {
    const auto* s = ds.station(id);
    return s ? static_cast<std::size_t>(s - ds.stations().data()) : graph::kNone;
}

json station_ref(const data::Station& s) {
    return {{"id", s.id}, {"name", s.name}, {"lat", s.pos.lat}, {"lng", s.pos.lng}};
}

}  // namespace

std::string Closure::id() const {
    return type == Type::Station ? "station:" + station : "segment:" + line + ":" + a + ":" + b;
}

Closure station_closure(const data::Dataset& ds, const std::string& station) {
    if (!ds.station(station)) throw ClosureError(404, "station_not_found", "no station with id " + station);
    Closure c;
    c.type = Closure::Type::Station;
    c.station = station;
    return c;
}

Closure segment_closure(const data::Dataset& ds, const graph::Graph& transit, const std::string& line,
                        const std::string& a, const std::string& b) {
    if (!ds.line(line)) throw ClosureError(404, "line_not_found", "no line with id " + line);
    for (const auto* s : {&a, &b}) {
        if (!ds.station(*s)) throw ClosureError(404, "station_not_found", "no station with id " + *s);
    }
    if (a == b) throw ClosureError(400, "invalid_segment", "a segment needs two different stations");
    const auto pa = transit.find(a + "@" + line);
    const auto pb = transit.find(b + "@" + line);
    const bool adjacent = pa && pb && std::any_of(transit.out_edges(*pa).begin(), transit.out_edges(*pa).end(),
                                                  [&](graph::EdgeId e) {
                                                      const auto& edge = transit.edge(e);
                                                      return edge.kind == EdgeKind::Ride && edge.to == *pb;
                                                  });
    if (!adjacent) {
        throw ClosureError(400, "invalid_segment", a + " and " + b + " are not neighbouring stations on " + line);
    }
    Closure c;
    c.type = Closure::Type::Segment;
    c.line = line;
    c.a = std::min(a, b);
    c.b = std::max(a, b);
    return c;
}

std::vector<char> blocked_edges(const graph::Graph& g, const std::vector<Closure>& closures) {
    std::vector<char> mask(g.edge_count(), 0);
    if (closures.empty()) return mask;

    // Closed stations by dataset index, and closed segments as platform pairs.
    std::vector<char> closed_station;
    std::vector<std::pair<graph::NodeId, graph::NodeId>> cut;
    for (const auto& c : closures) {
        if (c.type == Closure::Type::Station) {
            const auto gate = g.find(c.station);
            if (!gate) continue;
            const auto si = g.node(*gate).station;
            if (si == graph::kNone) continue;
            if (closed_station.size() <= si) closed_station.resize(si + 1, 0);
            closed_station[si] = 1;
        } else {
            const auto pa = g.find(c.a + "@" + c.line);
            const auto pb = g.find(c.b + "@" + c.line);
            if (pa && pb) cut.emplace_back(*pa, *pb);
        }
    }
    auto closed = [&](std::size_t si) { return si < closed_station.size() && closed_station[si]; };

    for (graph::EdgeId e = 0; e < g.edge_count(); ++e) {
        const auto& edge = g.edge(e);
        switch (edge.kind) {
            case EdgeKind::Board:
            case EdgeKind::Alight:
            case EdgeKind::Transfer:
                // All three happen inside one station: both ends carry its index.
                mask[e] = closed(g.node(edge.from).station);
                break;
            case EdgeKind::Ride:
                mask[e] = std::any_of(cut.begin(), cut.end(), [&](const auto& p) {
                    return (edge.from == p.first && edge.to == p.second) || (edge.from == p.second && edge.to == p.first);
                });
                break;
            default:
                break;
        }
    }
    return mask;
}

Store::Store(std::shared_ptr<const data::Dataset> ds, std::shared_ptr<const graph::Graph> transit)
    : ds_(std::move(ds)), transit_(std::move(transit)) {
    std::lock_guard lock(mu_);
    publish({});
}

std::shared_ptr<const Snapshot> Store::snapshot() const {
    std::lock_guard lock(mu_);
    return current_;
}

bool Store::add(const Closure& c) {
    std::lock_guard lock(mu_);
    auto closures = current_->closures;
    if (std::find(closures.begin(), closures.end(), c) != closures.end()) return false;
    if (closures.size() >= kMaxClosures) {
        throw ClosureError(409, "too_many_closures",
                           "at most " + std::to_string(kMaxClosures) + " closures at a time; reopen some first");
    }
    closures.push_back(c);
    publish(std::move(closures));
    return true;
}

bool Store::remove(const std::string& id) {
    std::lock_guard lock(mu_);
    auto closures = current_->closures;
    const auto it = std::find_if(closures.begin(), closures.end(), [&](const Closure& c) { return c.id() == id; });
    if (it == closures.end()) return false;
    closures.erase(it);
    publish(std::move(closures));
    return true;
}

void Store::clear() {
    std::lock_guard lock(mu_);
    if (!current_->closures.empty()) publish({});
}

void Store::publish(std::vector<Closure> closures) {
    auto next = std::make_shared<Snapshot>();
    next->blocked_edges = blocked_edges(*transit_, closures);
    next->closed_stations.assign(ds_->stations().size(), 0);
    for (const auto& c : closures) {
        if (c.type != Closure::Type::Station) continue;
        const auto si = station_index(*ds_, c.station);
        if (si != graph::kNone) next->closed_stations[si] = 1;
    }
    next->closures = std::move(closures);
    next->version = current_ ? current_->version + 1 : 0;
    current_ = std::move(next);
}

json to_json(const Closure& c, const data::Dataset& ds) {
    json j = {{"id", c.id()}};
    if (c.type == Closure::Type::Station) {
        const auto* s = ds.station(c.station);
        j["type"] = "station";
        j["station"] = station_ref(*s);
        j["label"] = s->name + " closed (trains do not stop)";
        return j;
    }
    const auto* l = ds.line(c.line);
    const auto* a = ds.station(c.a);
    const auto* b = ds.station(c.b);
    j["type"] = "segment";
    j["line"] = {{"id", l->id}, {"name", l->name}, {"color", l->color}};
    j["from"] = station_ref(*a);
    j["to"] = station_ref(*b);
    j["label"] = l->name + ": no trains between " + a->name + " and " + b->name;
    return j;
}

json to_json(const Snapshot& s, const data::Dataset& ds) {
    json items = json::array();
    for (const auto& c : s.closures) items.push_back(to_json(c, ds));
    return {{"items", std::move(items)}, {"version", s.version}};
}

}  // namespace safar::disruption
