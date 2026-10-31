#include "assistant/rank.hpp"

#include <algorithm>
#include <numeric>
#include <tuple>

namespace safar::assistant {

using nlohmann::json;

RouteFacts facts_from_json(const json& route) {
    RouteFacts f;
    f.minutes = route.value("total_min", 0.0);
    f.fare_rs = route.value("fare_rs", 0.0);
    f.transfers = route.value("transfers", 0);
    f.walk_min = route.value("walk_min", 0.0);
    f.score = route.value("score", 0.0);
    if (route.contains("legs")) {
        for (const auto& leg : route["legs"]) {
            const auto kind = leg.value("kind", std::string());
            if (kind == "metro") f.metro = true;
            if (kind != "road") continue;
            const auto mode = leg.value("mode", std::string());
            if (mode == "cab") f.cab = true;
            else if (mode == "auto") f.autorickshaw = true;
            else if (mode == "bike") f.bike = true;
        }
    }
    return f;
}

const char* to_string(SortKey k) {
    switch (k) {
        case SortKey::Score: return "best_balance";
        case SortKey::Price: return "price";
        case SortKey::Time: return "time";
        case SortKey::Changes: return "changes";
        case SortKey::Walking: return "walking";
    }
    return "best_balance";
}

namespace {

std::tuple<double, double, double> key(const RouteFacts& r, SortKey k) {
    switch (k) {
        case SortKey::Price: return {r.fare_rs, r.minutes, r.score};
        case SortKey::Time: return {r.minutes, r.fare_rs, r.score};
        case SortKey::Changes: return {r.transfers, r.minutes, r.fare_rs};
        case SortKey::Walking: return {r.walk_min + 4.0 * r.transfers, r.minutes, r.fare_rs};  // a change ~ 4 min on foot
        case SortKey::Score: break;
    }
    return {r.score, r.minutes, r.fare_rs};
}

SortKey objective_key(Objective o) {
    switch (o) {
        case Objective::Cheapest: return SortKey::Price;
        case Objective::Fastest: return SortKey::Time;
        case Objective::FewestChanges: return SortKey::Changes;
        case Objective::Comfort: return SortKey::Walking;
        case Objective::Balanced: break;
    }
    return SortKey::Score;
}

}  // namespace

Ranking rank(const std::vector<RouteFacts>& routes, const TripState& goal) {
    Ranking r;
    const bool budget = goal.budget_rs.has_value();
    const bool limit = goal.max_minutes.has_value();
    const bool stated = goal.objective != Objective::Balanced;

    if (stated) r.pick = r.list = objective_key(goal.objective);
    else if (budget && !limit) r.pick = SortKey::Time, r.list = SortKey::Price;
    else if (limit && !budget) r.pick = SortKey::Price, r.list = SortKey::Time;
    else if (budget && limit) r.pick = SortKey::Score, r.list = SortKey::Price;

    r.fit.resize(routes.size());
    for (std::size_t i = 0; i < routes.size(); ++i) {
        const auto& x = routes[i];
        auto& f = r.fit[i];
        // Fares are whole rupees and times are shown to the minute: half a unit of slack.
        if (budget && x.fare_rs > *goal.budget_rs + 0.5) f.over_budget_rs = x.fare_rs - *goal.budget_rs;
        if (limit && x.minutes > *goal.max_minutes + 0.5) f.over_time_min = x.minutes - *goal.max_minutes;
        const auto& n = goal.needs;
        f.missing_mode = (n.metro && !x.metro) || (n.cab && !x.cab) || (n.autorickshaw && !x.autorickshaw) || (n.bike && !x.bike);
        f.fits = f.over_budget_rs <= 0 && f.over_time_min <= 0 && !f.missing_mode;
        if (f.fits) ++r.feasible;
    }
    if (routes.empty()) return r;

    std::vector<std::size_t> idx(routes.size());
    std::iota(idx.begin(), idx.end(), 0);
    auto by = [&](SortKey k) { return [&, k](std::size_t a, std::size_t b) { return key(routes[a], k) < key(routes[b], k); }; };

    std::size_t top = 0;
    if (r.feasible > 0) {
        std::vector<std::size_t> ok;
        for (auto i : idx) {
            if (r.fit[i].fits) ok.push_back(i);
        }
        top = *std::min_element(ok.begin(), ok.end(), by(r.pick));
    } else {
        // Nothing fits: lead with the route closest to what was asked.
        const SortKey closest = budget ? SortKey::Price : limit ? SortKey::Time : r.list;
        top = *std::min_element(idx.begin(), idx.end(), by(closest));
    }

    std::vector<std::size_t> rest;
    for (auto i : idx) {
        if (i != top) rest.push_back(i);
    }
    std::stable_sort(rest.begin(), rest.end(), [&](std::size_t a, std::size_t b) {
        if (r.fit[a].fits != r.fit[b].fits) return r.fit[a].fits;
        return key(routes[a], r.list) < key(routes[b], r.list);
    });
    r.order.push_back(top);
    r.order.insert(r.order.end(), rest.begin(), rest.end());
    return r;
}

}  // namespace safar::assistant
