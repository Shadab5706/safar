#include "journey/planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>

#include "search/search.hpp"
#include "search/yen.hpp"

namespace safar::journey {

using graph::EdgeId;
using graph::EdgeKind;
using graph::NodeId;
using nlohmann::json;

namespace {

double round1(double x) { return std::round(x * 10.0) / 10.0; }

// Two routes with the same skeleton differ only in which vehicle carries a road leg.
// They are merged into one route that lists the vehicles as options.
std::string skeleton(const search::Itinerary& it) {
    std::string key;
    for (const auto& l : it.legs) {
        switch (l.kind) {
            case search::Leg::Kind::Metro: key += "M" + std::to_string(l.line); break;
            case search::Leg::Kind::Road: key += "R"; break;
            case search::Leg::Kind::Walk: key += "W"; break;
            case search::Leg::Kind::Transfer: continue;
        }
        key += ":" + std::to_string(l.from) + ">" + std::to_string(l.to) + ";";
    }
    return key;
}

// Routes on the same sequence of lines/modes within this much of a better route are
// near-duplicates (e.g. boarding the Aqua Line one station earlier) and are dropped.
constexpr double kDupTimeMin = 10.0;
constexpr double kDupFareRs = 20.0;

std::string mode_signature(const search::Itinerary& it) {
    std::string sig;
    for (const auto& l : it.legs) {
        if (l.kind == search::Leg::Kind::Metro) sig += "M" + std::to_string(l.line) + ";";
        else if (l.kind == search::Leg::Kind::Road) sig += "R;";
        else if (l.kind == search::Leg::Kind::Walk && l.walk_min >= 1) sig += "W;";
    }
    return sig;
}

bool is_direct_road(const search::Itinerary& it) {
    return it.legs.size() == 1 && it.legs[0].kind == search::Leg::Kind::Road;
}

// Differences smaller than this don't count: a route 1 minute faster but ₹40 dearer
// is not a real alternative, it is clutter.
constexpr double kTimeSlackMin = 2.0;
constexpr double kFareSlackRs = 5.0;

// b dominates a: practically no worse on time, fare and changes, and clearly better
// on at least one. The slack only ever helps b on time, and "clearly better" is
// strict, so two routes can never dominate each other.
bool dominates(const search::Itinerary& b, const search::Itinerary& a) {
    const bool no_worse =
        b.total_min <= a.total_min + kTimeSlackMin && b.fare_rs <= a.fare_rs && b.transfers <= a.transfers;
    const bool better = b.total_min < a.total_min - kTimeSlackMin || b.fare_rs < a.fare_rs - kFareSlackRs ||
                        b.transfers < a.transfers;
    return no_worse && better;
}

const char* road_label(graph::RoadMode m) {
    switch (m) {
        case graph::RoadMode::Cab: return "Cab";
        case graph::RoadMode::Auto: return "Auto";
        case graph::RoadMode::Bike: return "Bike taxi";
    }
    return "Cab";
}

// Public name of a place or station node, used to label deep links.
std::string node_name(const graph::Graph& g, const data::Dataset& ds, NodeId n) {
    const auto& node = g.node(n);
    if (node.place != graph::kNone) return ds.places()[node.place].name;
    if (node.station != graph::kNone) return ds.stations()[node.station].name;
    return {};
}

std::string summary(const search::Itinerary& it, const data::Dataset& ds) {
    std::string s;
    for (const auto& l : it.legs) {
        std::string part;
        if (l.kind == search::Leg::Kind::Metro) {
            const auto& name = ds.lines()[l.line].name;
            part = name.substr(0, name.find(" ("));
        } else if (l.kind == search::Leg::Kind::Road) {
            part = road_label(l.road_mode);
        } else if (l.kind == search::Leg::Kind::Walk && l.walk_min >= 1) {
            part = "Walk";
        }
        if (part.empty()) continue;
        if (!s.empty()) s += " → ";
        s += part;
    }
    return s;
}

}  // namespace

bool ModeSet::road(graph::RoadMode m) const {
    switch (m) {
        case graph::RoadMode::Cab: return cab;
        case graph::RoadMode::Auto: return autorickshaw;
        case graph::RoadMode::Bike: return bike;
    }
    return false;
}

Planner::Planner(std::shared_ptr<const data::Dataset> ds, std::shared_ptr<const fares::TransportConfig> cfg,
                 std::shared_ptr<const roads::RoadMatrix> roads, std::shared_ptr<const graph::Graph> transit,
                 std::shared_ptr<const fares::FareEngine> fares)
    : ds_(std::move(ds)), cfg_(std::move(cfg)), roads_(std::move(roads)), transit_(std::move(transit)),
      fares_(std::move(fares)) {
    if (!fares_) throw std::invalid_argument("Planner needs a fare engine");
}

Planner::Point Planner::point(const Endpoint& e) const {
    if (e.type == Endpoint::Type::Station) {
        const auto& s = ds_->stations()[e.index];
        return {0, s.pos, roads::station_key(s.id)};
    }
    const auto& p = ds_->places()[e.index];
    return {0, p.pos, roads::place_key(p.id), e.index};
}

fares::Trip Planner::road_trip(const Endpoint& from, const Endpoint& to, int hour) const {
    const auto name = [&](const Endpoint& e) {
        return e.type == Endpoint::Type::Station ? ds_->stations()[e.index].name : ds_->places()[e.index].name;
    };
    const auto leg = road_leg(point(from), point(to));
    return {point(from).pos, point(to).pos, name(from), name(to), leg.km, leg.free_flow_min, hour, leg.from_osrm};
}

Planner::Point Planner::attach(graph::Graph& g, const Endpoint& e) const {
    if (e.type == Endpoint::Type::Station) return station_point(g, e.index);
    const auto& p = ds_->places()[e.index];
    const NodeId n = g.add_node({"place:" + p.id, graph::NodeKind::Place, p.pos, graph::kNone, graph::kNone, e.index});
    return {n, p.pos, roads::place_key(p.id), e.index};
}

Planner::Point Planner::station_point(const graph::Graph& g, std::size_t station) const {
    const auto& s = ds_->stations()[station];
    return {*g.find(s.id), s.pos, roads::station_key(s.id)};
}

// The nearest few stations, plus the nearest station of every other line within
// reach, so "cab to the Magenta Line" is considered even when it isn't the closest.
std::vector<std::size_t> Planner::access_stations(geo::LatLng pos) const {
    const auto& st = ds_->stations();
    std::vector<std::pair<double, std::size_t>> by_dist;
    by_dist.reserve(st.size());
    for (std::size_t i = 0; i < st.size(); ++i) by_dist.emplace_back(geo::haversine_km(pos, st[i].pos), i);
    std::sort(by_dist.begin(), by_dist.end());

    std::vector<std::size_t> out;
    std::set<std::string> lines_seen;
    for (std::size_t k = 0; k < by_dist.size(); ++k) {
        const auto [km, i] = by_dist[k];
        const bool nearest = k < cfg_->nearest_stations;
        if (!nearest && km > cfg_->line_radius_km) break;
        const bool new_line = std::any_of(st[i].lines.begin(), st[i].lines.end(),
                                          [&](const std::string& l) { return !lines_seen.count(l); });
        if (nearest || new_line) {
            out.push_back(i);
            lines_seen.insert(st[i].lines.begin(), st[i].lines.end());
        }
    }
    return out;
}

roads::RoadLeg Planner::road_leg(const Point& a, const Point& b) const {
    if (auto leg = roads_->lookup(a.key, b.key)) return *leg;
    const double km = geo::haversine_km(a.pos, b.pos) * cfg_->fallback_detour;
    return {km, km / cfg_->fallback_speed_kmh * 60.0, false};
}

// Adds a -> b edges for walking (if short enough) and for every enabled road mode.
void Planner::add_links(graph::Graph& g, const Point& a, const Point& b, const JourneyRequest& req,
                        bool access) const {
    const double straight_km = geo::haversine_km(a.pos, b.pos);
    double max_walk_km = access ? cfg_->walk.max_access_km : cfg_->walk.max_direct_km;
    for (const Point* p : {&a, &b}) {
        if (p->place == graph::kNone) continue;
        const auto& limits = cfg_->walk.max_km_by_category;
        if (auto it = limits.find(ds_->places()[p->place].category); it != limits.end()) {
            max_walk_km = std::min(max_walk_km, it->second);
        }
    }
    if (req.modes.walk && straight_km <= max_walk_km) {
        const double km = straight_km * cfg_->walk.detour_factor;
        const double minutes = std::max(0.5, km / cfg_->walk.speed_kmh * 60.0);
        g.add_edge({a.node, b.node, EdgeKind::Walk, minutes, 0, km, graph::kNone});
    }
    if (!req.modes.any_road()) return;

    const auto leg = road_leg(a, b);
    const fares::Trip trip{a.pos, b.pos, {}, {}, leg.km, leg.free_flow_min, req.hour, leg.from_osrm};
    for (auto mode : graph::kAllRoadModes) {
        if (!req.modes.road(mode)) continue;
        const auto& p = cfg_->road_params(mode);
        if (leg.km > (access ? p.max_access_km : p.max_km)) continue;
        const auto ride = fares_->ride(mode, trip);
        const double fare = fares_->search_fare(mode, ride);  // cheapest product of this vehicle type
        g.add_edge({a.node, b.node, EdgeKind::Road, p.pickup_min + ride.ride_min, p.pickup_min, leg.km, graph::kNone,
                    fare, mode, leg.from_osrm});
    }
}

JourneyResult Planner::plan(const JourneyRequest& req) const {
    if (req.from == req.to) throw PlanError(400, "same_endpoints", "from and to are the same place");
    if (req.hour < 0 || req.hour > 23) throw PlanError(400, "invalid_hour", "hour must be 0-23");

    JourneyResult out;
    out.graph = *transit_;  // per-query copy: ~600 nodes, cheap, and keeps the shared graph read-only
    auto& g = out.graph;
    const Point origin = attach(g, req.from);
    const Point dest = attach(g, req.to);
    out.origin = origin.node;
    out.destination = dest.node;

    // ---- 1. connect the endpoints to the network ----
    const auto* closures = req.closures;
    auto closed = [&](std::size_t si) { return closures && closures->station_closed(si); };
    std::vector<Point> origin_access, dest_access;
    if (req.modes.metro) {
        if (req.from.type == Endpoint::Type::Place) {
            for (auto si : access_stations(origin.pos)) {
                if (closed(si)) continue;
                const auto st = station_point(g, si);
                if (st.node == dest.node) continue;
                add_links(g, origin, st, req, true);
                origin_access.push_back(st);
            }
        }
        if (req.to.type == Endpoint::Type::Place) {
            for (auto si : access_stations(dest.pos)) {
                if (closed(si)) continue;
                const auto st = station_point(g, si);
                if (st.node == origin.node) continue;
                add_links(g, st, dest, req, true);
                dest_access.push_back(st);
            }
        }
    }
    add_links(g, origin, dest, req, false);

    std::vector<char> mask(g.edge_count(), 0);  // honours "no metro" and line closures
    std::vector<char> transit_only(g.edge_count(), 0);  // walk + metro only
    for (EdgeId e = 0; e < g.edge_count(); ++e) {
        const auto kind = g.edge(e).kind;
        if (!req.modes.metro && kind == EdgeKind::Board) mask[e] = 1;
        // The copy starts with the shared graph's edges, so the closure mask lines up.
        if (closures && e < closures->blocked_edges.size() && closures->blocked_edges[e]) mask[e] = 1;
        transit_only[e] = mask[e] || kind == EdgeKind::Road;
    }

    // ---- 2. generate candidate routes ----
    const double vot = cfg_->value_of_time(req.preference);
    out.value_of_time = vot;

    struct Candidate {
        std::vector<EdgeId> path;
        std::string found_by;
    };
    std::vector<Candidate> cands;
    auto options = [&](double money_weight, double boarding_penalty, const std::vector<char>* blocked) {
        search::SearchOptions o;
        o.cost.money_weight = money_weight;
        o.cost.boarding_penalty = boarding_penalty;
        o.record_expansion = false;
        o.blocked_edges = blocked;
        return o;
    };
    auto run = [&](const char* name, const search::SearchOptions& o) {
        ++out.searches;
        auto r = search::a_star(g, origin.node, dest.node, o);
        if (r.found && !r.path.empty()) cands.push_back({std::move(r.path), name});
    };

    // Weighted-sum sweep: each value of time (₹ per minute) finds one point on the
    // time-vs-money trade-off curve, from "only money matters" to "only time matters".
    run("cheapest", options(20, 0, &mask));  // ₹1 = 20 min: money dominates, time breaks ties
    for (double v : {0.5, 1.0, 2.0, 4.0, 8.0, 15.0, 30.0}) run("tradeoff", options(1.0 / v, 0, &mask));
    run("fastest", options(0, 0, &mask));
    run("balanced", options(1.0 / vot, 0, &mask));
    run("fewest_changes", options(1.0 / vot, 15, &mask));
    if (req.modes.metro && req.modes.any_road()) run("public_transport", options(1.0 / vot, 0, &transit_only));

    ++out.searches;
    for (auto& alt : search::k_shortest_paths(g, origin.node, dest.node, 12, options(1.0 / vot, 0, &mask))) {
        cands.push_back({std::move(alt.path), "alternative"});
    }
    for (EdgeId e : g.out_edges(origin.node)) {
        if (g.edge(e).to == dest.node) cands.push_back({{e}, "direct"});
    }

    // Via-station alternatives ("cab to Botanical Garden, then the Magenta Line").
    // The sweep above only finds routes on the convex hull of the time/money
    // trade-off; forcing the first or last leg through each access station also finds
    // the Pareto-optimal routes inside it.
    const auto balanced = options(1.0 / vot, 0, &mask);
    auto best_link = [&](NodeId a, NodeId b) -> std::optional<EdgeId> {
        std::optional<EdgeId> best;
        double best_cost = std::numeric_limits<double>::infinity();
        for (EdgeId e : g.out_edges(a)) {
            if (g.edge(e).to != b || mask[e]) continue;
            const double c = balanced.cost.edge_cost(g.edge(e));
            if (c < best_cost) {
                best_cost = c;
                best = e;
            }
        }
        return best;
    };
    for (const auto& st : origin_access) {
        const auto link = best_link(origin.node, st.node);
        if (!link) continue;
        ++out.searches;
        auto rest = search::a_star(g, st.node, dest.node, balanced);
        if (!rest.found) continue;
        std::vector<EdgeId> path{*link};
        path.insert(path.end(), rest.path.begin(), rest.path.end());
        cands.push_back({std::move(path), "via_station"});
    }
    for (const auto& st : dest_access) {
        const auto link = best_link(st.node, dest.node);
        if (!link) continue;
        ++out.searches;
        auto first = search::a_star(g, origin.node, st.node, balanced);
        if (!first.found) continue;
        first.path.push_back(*link);
        cands.push_back({std::move(first.path), "via_station"});
    }
    out.candidates = cands.size();
    if (cands.empty()) {
        throw PlanError(422, "no_route", "no route found with the selected modes");
    }

    // ---- 3. price, merge vehicle variants, keep the best of each ----
    std::map<std::string, Route> by_skeleton;
    for (auto& c : cands) {
        Route r;
        r.itinerary = search::build_itinerary(g, c.path);
        search::price_itinerary(r.itinerary, *ds_, *cfg_, req.sunday);
        r.score = r.itinerary.total_min + r.itinerary.fare_rs / vot;
        r.found_by = c.found_by;
        auto key = skeleton(r.itinerary);
        auto it = by_skeleton.find(key);
        if (it == by_skeleton.end() || r.score < it->second.score) by_skeleton[key] = std::move(r);
    }
    std::vector<Route> all;
    for (auto& [key, r] : by_skeleton) all.push_back(std::move(r));

    // Drop near-duplicates, keeping the better-scoring one.
    std::sort(all.begin(), all.end(), [](const Route& a, const Route& b) { return a.score < b.score; });
    std::vector<Route> unique;
    for (auto& r : all) {
        const auto sig = mode_signature(r.itinerary);
        const bool near_dup = std::any_of(unique.begin(), unique.end(), [&](const Route& u) {
            return mode_signature(u.itinerary) == sig &&
                   std::abs(u.itinerary.total_min - r.itinerary.total_min) <= kDupTimeMin &&
                   std::abs(u.itinerary.fare_rs - r.itinerary.fare_rs) <= kDupFareRs;
        });
        if (!near_dup) unique.push_back(std::move(r));
    }
    all = std::move(unique);
    out.distinct = all.size();

    // Every provider's quote for every road leg, from the vehicle types the leg allows.
    const double traffic = cfg_->traffic[static_cast<std::size_t>(req.hour)];
    for (auto& r : all) {
        r.road_options.resize(r.itinerary.legs.size());
        for (std::size_t i = 0; i < r.itinerary.legs.size(); ++i) {
            const auto& leg = r.itinerary.legs[i];
            if (leg.kind != search::Leg::Kind::Road) continue;
            for (EdgeId e : g.out_edges(leg.from)) {
                const auto& edge = g.edge(e);
                if (edge.to != leg.to || edge.kind != EdgeKind::Road) continue;
                const double ride_min = edge.time_min - edge.wait_min;
                // Same ride the edge was priced with, so the leg fare is the cheapest quote of its mode.
                const fares::Ride ride{g.node(leg.from).pos, g.node(leg.to).pos, node_name(g, *ds_, leg.from),
                                       node_name(g, *ds_, leg.to), edge.distance_km, ride_min, ride_min / traffic,
                                       req.hour};
                auto quotes = fares_->quotes(edge.road_mode, ride);
                r.road_options[i].insert(r.road_options[i].end(), quotes.begin(), quotes.end());
            }
            fares::sort_quotes(r.road_options[i]);
        }
    }

    // ---- 4. Pareto filter on (time, fare, changes) ----
    std::vector<char> dominated(all.size(), 0);
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = 0; j < all.size() && !dominated[i]; ++j) {
            dominated[i] = i != j && dominates(all[j].itinerary, all[i].itinerary);
        }
    }
    std::vector<Route> front;
    std::optional<Route> direct_road;  // "what would a cab all the way cost?" is always worth showing
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (!dominated[i]) {
            front.push_back(std::move(all[i]));
        } else if (is_direct_road(all[i].itinerary)) {
            all[i].dominated = true;
            direct_road = std::move(all[i]);
        }
    }
    out.pareto = front.size();

    // ---- 5. tag and pick ----
    auto pick = [&](auto better) {
        return static_cast<std::size_t>(std::min_element(front.begin(), front.end(), better) - front.begin());
    };
    const auto I = [](const Route& r) -> const search::Itinerary& { return r.itinerary; };
    const std::size_t recommended = pick([](const Route& a, const Route& b) { return a.score < b.score; });
    const std::size_t fastest = pick([&](const Route& a, const Route& b) { return I(a).total_min < I(b).total_min; });
    const std::size_t cheapest = pick([&](const Route& a, const Route& b) {
        return I(a).fare_rs != I(b).fare_rs ? I(a).fare_rs < I(b).fare_rs : I(a).total_min < I(b).total_min;
    });
    const std::size_t fewest = pick([&](const Route& a, const Route& b) {
        return I(a).transfers != I(b).transfers ? I(a).transfers < I(b).transfers : a.score < b.score;
    });

    front[recommended].tags.push_back("recommended");
    front[fastest].tags.push_back("fastest");
    front[cheapest].tags.push_back("cheapest");
    if (front.size() > 1) front[fewest].tags.push_back("fewest_changes");

    // "Most expensive" is judged over everything shown, including a beaten direct cab.
    if (direct_road) front.push_back(std::move(*direct_road));
    const std::size_t priciest = pick([&](const Route& a, const Route& b) {
        return I(a).fare_rs != I(b).fare_rs ? I(a).fare_rs > I(b).fare_rs : I(a).total_min < I(b).total_min;
    });
    if (front.size() > 1 && I(front[priciest]).fare_rs > I(front[cheapest]).fare_rs) {
        front[priciest].tags.push_back("most_expensive");
    }

    // Tagged routes first (recommended leads), then the rest by score, up to max_results.
    std::vector<std::size_t> order(front.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const bool ta = !front[a].tags.empty() || front[a].dominated;
        const bool tb = !front[b].tags.empty() || front[b].dominated;
        if (a == recommended || b == recommended) return a == recommended && b != recommended;
        if (ta != tb) return ta;
        return front[a].score < front[b].score;
    });
    order.resize(std::min(order.size(), std::max<std::size_t>(1, req.max_results)));
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return front[a].score < front[b].score; });
    for (auto i : order) out.routes.push_back(std::move(front[i]));
    return out;
}

json to_json(const JourneyResult& r, const data::Dataset& ds) {
    json routes = json::array();
    for (std::size_t i = 0; i < r.routes.size(); ++i) {
        const auto& route = r.routes[i];
        json j = search::to_json(route.itinerary, r.graph, ds);
        for (std::size_t li = 0; li < route.road_options.size(); ++li) {
            if (route.road_options[li].empty()) continue;
            json opts = json::array();
            for (const auto& o : route.road_options[li]) opts.push_back(fares::to_json(o));
            j["legs"][li]["options"] = std::move(opts);
        }
        j["rank"] = i + 1;
        j["tags"] = route.tags;
        j["summary"] = summary(route.itinerary, ds);
        j["score"] = round1(route.score);
        j["found_by"] = route.found_by;
        j["dominated"] = route.dominated;
        routes.push_back(std::move(j));
    }
    return {{"from", search::node_ref(r.graph, ds, r.origin)},
            {"to", search::node_ref(r.graph, ds, r.destination)},
            {"value_of_time_rs_per_min", round1(r.value_of_time)},
            {"routes", std::move(routes)},
            {"stats", {{"searches", r.searches}, {"candidates", r.candidates}, {"distinct", r.distinct}, {"pareto", r.pareto}}}};
}

}  // namespace safar::journey
