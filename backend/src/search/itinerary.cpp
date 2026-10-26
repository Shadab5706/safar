#include "search/itinerary.hpp"

#include <cmath>

namespace safar::search {

using nlohmann::json;

namespace {

double round1(double x) { return std::round(x * 10.0) / 10.0; }

json station_ref(const data::Dataset& ds, std::size_t idx) {
    const auto& s = ds.stations()[idx];
    return {{"id", s.id}, {"name", s.name}};
}

}  // namespace

Itinerary build_itinerary(const graph::Graph& g, const std::vector<graph::EdgeId>& path) {
    Itinerary it;
    Leg cur;
    bool in_train = false;

    auto open_metro = [&](std::size_t station, std::size_t line) {
        cur = Leg{};
        cur.kind = Leg::Kind::Metro;
        cur.line = line;
        cur.from_station = cur.to_station = station;
        cur.stations = {station};
        in_train = true;
    };
    auto close_metro = [&] {
        if (in_train) it.legs.push_back(cur);
        in_train = false;
    };

    for (auto eid : path) {
        const auto& e = g.edge(eid);
        const auto& to = g.node(e.to);
        const auto& from = g.node(e.from);
        it.total_min += e.time_min;
        it.distance_km += e.distance_km;

        switch (e.kind) {
            case graph::EdgeKind::Board:
                open_metro(to.station, e.line);
                cur.entry_min = e.time_min - e.wait_min;
                cur.wait_min = e.wait_min;
                break;
            case graph::EdgeKind::Ride:
                cur.ride_min += e.time_min;
                cur.distance_km += e.distance_km;
                cur.stops += 1;
                cur.to_station = to.station;
                cur.stations.push_back(to.station);
                break;
            case graph::EdgeKind::Transfer: {
                close_metro();
                Leg t;
                t.kind = Leg::Kind::Transfer;
                t.from_station = t.to_station = to.station;
                t.stations = {to.station};
                t.walk_min = e.time_min - e.wait_min;
                it.legs.push_back(t);
                open_metro(to.station, e.line);
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
                w.from_station = from.station;
                w.to_station = to.station;
                w.stations = {from.station, to.station};
                w.walk_min = e.time_min;
                w.distance_km = e.distance_km;
                it.legs.push_back(w);
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

json to_json(const Itinerary& it, const data::Dataset& ds) {
    json legs = json::array();
    for (const auto& l : it.legs) {
        json j;
        switch (l.kind) {
            case Leg::Kind::Metro: {
                const auto& line = ds.lines()[l.line];
                j = {{"kind", "metro"},
                     {"line", {{"id", line.id}, {"name", line.name}, {"color", line.color}, {"mode", data::to_string(line.mode)}}},
                     {"from", station_ref(ds, l.from_station)},
                     {"to", station_ref(ds, l.to_station)},
                     {"stops", l.stops},
                     {"entry_min", round1(l.entry_min)},
                     {"wait_min", round1(l.wait_min)},
                     {"ride_min", round1(l.ride_min)},
                     {"exit_min", round1(l.exit_min)},
                     {"distance_km", round1(l.distance_km)}};
                break;
            }
            case Leg::Kind::Transfer:
                j = {{"kind", "transfer"}, {"station", station_ref(ds, l.from_station)}, {"walk_min", round1(l.walk_min)}};
                break;
            case Leg::Kind::Walk:
                j = {{"kind", "walk"},
                     {"from", station_ref(ds, l.from_station)},
                     {"to", station_ref(ds, l.to_station)},
                     {"walk_min", round1(l.walk_min)},
                     {"distance_km", round1(l.distance_km)}};
                break;
        }
        json stations = json::array();
        for (auto s : l.stations) stations.push_back(ds.stations()[s].id);
        j["stations"] = std::move(stations);
        j["total_min"] = round1(l.total_min());
        legs.push_back(std::move(j));
    }
    return {{"total_min", round1(it.total_min)},
            {"distance_km", round1(it.distance_km)},
            {"transfers", it.transfers},
            {"legs", std::move(legs)}};
}

}  // namespace safar::search
