#include "search/itinerary.hpp"

#include <cmath>

#include "fares/transport_config.hpp"

namespace safar::search {

using nlohmann::json;

namespace {

double round1(double x) { return std::round(x * 10.0) / 10.0; }

}  // namespace

Itinerary build_itinerary(const graph::Graph& g, const std::vector<graph::EdgeId>& path) {
    Itinerary it;
    Leg cur;
    bool in_train = false;

    auto open_metro = [&](graph::NodeId platform, std::size_t line) {
        cur = Leg{};
        cur.kind = Leg::Kind::Metro;
        cur.line = line;
        cur.from = cur.to = platform;
        cur.nodes = {platform};
        in_train = true;
    };
    auto close_metro = [&] {
        if (in_train) it.legs.push_back(cur);
        in_train = false;
    };

    for (auto eid : path) {
        const auto& e = g.edge(eid);
        it.total_min += e.time_min;
        it.distance_km += e.distance_km;

        switch (e.kind) {
            case graph::EdgeKind::Board:
                open_metro(e.to, e.line);
                cur.entry_min = e.time_min - e.wait_min;
                cur.wait_min = e.wait_min;
                break;
            case graph::EdgeKind::Ride:
                cur.ride_min += e.time_min;
                cur.distance_km += e.distance_km;
                cur.stops += 1;
                cur.to = e.to;
                cur.nodes.push_back(e.to);
                break;
            case graph::EdgeKind::Transfer: {
                close_metro();
                Leg t;
                t.kind = Leg::Kind::Transfer;
                t.from = e.from;
                t.to = e.to;
                t.nodes = {e.from};
                t.walk_min = e.time_min - e.wait_min;
                it.walk_min += t.walk_min;
                it.legs.push_back(t);
                open_metro(e.to, e.line);
                cur.wait_min = e.wait_min;
                break;
            }
            case graph::EdgeKind::Alight:
                cur.exit_min = e.time_min;
                close_metro();
                break;
            case graph::EdgeKind::Walk: {
                Leg w;
                w.kind = Leg::Kind::Walk;
                w.from = e.from;
                w.to = e.to;
                w.nodes = {e.from, e.to};
                w.walk_min = e.time_min;
                w.distance_km = e.distance_km;
                it.walk_min += w.walk_min;
                it.legs.push_back(w);
                break;
            }
            case graph::EdgeKind::Road: {
                Leg r;
                r.kind = Leg::Kind::Road;
                r.road_mode = e.road_mode;
                r.from = e.from;
                r.to = e.to;
                r.nodes = {e.from, e.to};
                r.wait_min = e.wait_min;
                r.ride_min = e.time_min - e.wait_min;
                r.distance_km = e.distance_km;
                r.fare_rs = e.money;
                r.road_from_osrm = e.from_osrm;
                it.legs.push_back(r);
                break;
            }
        }
    }
    close_metro();  // only matters for paths that end on a platform

    int metro_legs = 0;
    for (const auto& l : it.legs) metro_legs += (l.kind == Leg::Kind::Metro);
    it.transfers = metro_legs > 0 ? metro_legs - 1 : 0;
    return it;
}

void price_itinerary(Itinerary& it, const data::Dataset& ds, const fares::TransportConfig& cfg, bool sunday) {
    it.tickets.clear();
    it.fare_rs = 0;
    const Leg* prev = nullptr;
    for (auto& leg : it.legs) {
        if (leg.kind == Leg::Kind::Road) {
            leg.fare_rs = std::round(leg.fare_rs);
            it.fare_rs += leg.fare_rs;
        } else if (leg.kind == Leg::Kind::Metro) {
            const std::string zone = fares::fare_zone(ds.lines()[leg.line]);
            // Same ticket only when we changed trains inside the station (a Transfer
            // leg right before) and stayed with the same operator.
            const bool continues = prev && prev->kind == Leg::Kind::Transfer && !it.tickets.empty() &&
                                   it.tickets.back().zone == zone;
            if (!continues) it.tickets.push_back({zone, leg.from, leg.to, 0, 0, cfg.fare_table(zone).verified});
            auto& t = it.tickets.back();
            t.to = leg.to;
            t.distance_km += leg.distance_km;
            leg.ticket = static_cast<int>(it.tickets.size()) - 1;
        }
        prev = &leg;
    }
    for (auto& t : it.tickets) {
        t.fare_rs = cfg.fare_table(t.zone).fare(t.distance_km, sunday);
        it.fare_rs += t.fare_rs;
    }
}

json node_ref(const graph::Graph& g, const data::Dataset& ds, graph::NodeId n) {
    const auto& node = g.node(n);
    if (node.kind == graph::NodeKind::Place) {
        const auto& p = ds.places()[node.place];
        return {{"type", "place"}, {"id", p.id}, {"name", p.name}, {"lat", p.pos.lat}, {"lng", p.pos.lng}};
    }
    const auto& s = ds.stations()[node.station];
    return {{"type", "station"}, {"id", s.id}, {"name", s.name}, {"lat", s.pos.lat}, {"lng", s.pos.lng}};
}

json to_json(const Itinerary& it, const graph::Graph& g, const data::Dataset& ds) {
    json legs = json::array();
    for (const auto& l : it.legs) {
        json j;
        switch (l.kind) {
            case Leg::Kind::Metro: {
                const auto& line = ds.lines()[l.line];
                j = {{"kind", "metro"},
                     {"line", {{"id", line.id}, {"name", line.name}, {"color", line.color}, {"mode", data::to_string(line.mode)}}},
                     {"stops", l.stops},
                     {"entry_min", round1(l.entry_min)},
                     {"wait_min", round1(l.wait_min)},
                     {"ride_min", round1(l.ride_min)},
                     {"exit_min", round1(l.exit_min)},
                     {"ticket", l.ticket}};
                break;
            }
            case Leg::Kind::Transfer:
                j = {{"kind", "transfer"}, {"walk_min", round1(l.walk_min)}};
                break;
            case Leg::Kind::Walk:
                j = {{"kind", "walk"}, {"walk_min", round1(l.walk_min)}};
                break;
            case Leg::Kind::Road:
                j = {{"kind", "road"},
                     {"mode", graph::to_string(l.road_mode)},
                     {"wait_min", round1(l.wait_min)},
                     {"ride_min", round1(l.ride_min)},
                     {"fare_rs", l.fare_rs},
                     {"fare_source", "estimate"},
                     {"road_data", l.road_from_osrm ? "osrm" : "straight_line_estimate"}};
                break;
        }
        j["from"] = node_ref(g, ds, l.from);
        j["to"] = node_ref(g, ds, l.to);
        j["distance_km"] = round1(l.distance_km);
        j["total_min"] = round1(l.total_min());
        json points = json::array();
        for (auto n : l.nodes) points.push_back({g.node(n).pos.lat, g.node(n).pos.lng});
        j["points"] = std::move(points);
        legs.push_back(std::move(j));
    }

    json tickets = json::array();
    for (const auto& t : it.tickets) {
        tickets.push_back({{"zone", t.zone},
                           {"from", node_ref(g, ds, t.from)},
                           {"to", node_ref(g, ds, t.to)},
                           {"distance_km", round1(t.distance_km)},
                           {"fare_rs", t.fare_rs},
                           {"fare_verified", t.verified}});
    }

    return {{"total_min", round1(it.total_min)},
            {"distance_km", round1(it.distance_km)},
            {"fare_rs", it.fare_rs},
            {"walk_min", round1(it.walk_min)},
            {"transfers", it.transfers},
            {"legs", std::move(legs)},
            {"tickets", std::move(tickets)}};
}

}  // namespace safar::search
