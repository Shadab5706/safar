#include "assistant/reply.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace safar::assistant {

std::string format_minutes(double minutes) {
    const long m = std::lround(minutes);
    if (m < 60) return std::to_string(m) + " min";
    if (m % 60 == 0) return std::to_string(m / 60) + " h";
    char buf[48];  // room for any two longs, so GCC -Wformat-truncation stays quiet
    std::snprintf(buf, sizeof buf, "%ld h %ld min", m / 60, m % 60);
    return buf;
}

std::string format_rupees(double rupees) { return "₹" + std::to_string(std::lround(rupees)); }

std::string describe_departure(const std::string& depart_at, const std::string& now) {
    using namespace std::chrono;
    int y1 = 0, m1 = 0, d1 = 0, h = 0, mi = 0, y0 = 0, m0 = 0, d0 = 0;
    if (std::sscanf(depart_at.c_str(), "%4d-%2d-%2dT%2d:%2d", &y1, &m1, &d1, &h, &mi) != 5) return "at " + depart_at;
    std::sscanf(now.c_str(), "%4d-%2d-%2d", &y0, &m0, &d0);
    const sys_days day{year{y1} / static_cast<unsigned>(m1) / static_cast<unsigned>(d1)};
    const sys_days today{year{y0} / static_cast<unsigned>(m0) / static_cast<unsigned>(d0)};
    char clock[16];
    std::snprintf(clock, sizeof clock, "%d:%02d %s", h % 12 == 0 ? 12 : h % 12, mi, h < 12 ? "am" : "pm");
    const auto diff = (day - today).count();
    if (diff == 0) return std::string("today at ") + clock;
    if (diff == 1) return std::string("tomorrow at ") + clock;
    static const char* names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    return std::string("on ") + names[weekday{day}.c_encoding()] + " at " + clock;
}

namespace {

std::string route_line(const RouteFacts& r, const std::string& summary, bool money_first) {
    return summary + ": " + (money_first ? format_rupees(r.fare_rs) + " in " + format_minutes(r.minutes)
                                         : format_minutes(r.minutes) + " for " + format_rupees(r.fare_rs));
}

std::string count_of(std::size_t k, std::size_t n, const char* what) {
    if (k == n) return n == 1 ? std::string("The only route ") + what : std::string("All ") + std::to_string(n) + " routes " + what;
    return std::to_string(k) + " of " + std::to_string(n) + " routes " + what;
}

const char* rest_order(SortKey k) {
    switch (k) {
        case SortKey::Price: return "from cheapest to most expensive";
        case SortKey::Time: return "from quickest to slowest";
        case SortKey::Changes: return "by number of changes";
        case SortKey::Walking: return "by how much walking they need";
        case SortKey::Score: break;
    }
    return "by the best balance of time and money";
}

}  // namespace

std::string trip_reply(const TripState& goal, const std::string& from, const std::string& to,
                       const std::vector<RouteFacts>& routes, const std::vector<std::string>& summaries,
                       const Ranking& ranking) {
    if (routes.empty()) return "I couldn't find a route from " + from + " to " + to + ".";
    const std::size_t n = routes.size();
    const std::size_t first = ranking.order.front();
    const auto& best = routes[first];
    const auto& line = summaries[first];
    const bool budget = goal.budget_rs.has_value();
    const bool limit = goal.max_minutes.has_value();
    const std::string more = n > 1 ? " The other " + std::to_string(n - 1) + " follow " + rest_order(ranking.list) + "." : "";

    if ((budget || limit || goal.needs.any()) && ranking.feasible == 0) {
        if (budget && limit) {
            return "No route from " + from + " to " + to + " is both under " + format_rupees(*goal.budget_rs) + " and within " +
                   format_minutes(*goal.max_minutes) + ". The closest is first. " + route_line(best, line, true) + "." + more;
        }
        if (budget) {
            return "Nothing gets from " + from + " to " + to + " for " + format_rupees(*goal.budget_rs) +
                   " or less. The cheapest way is first. " + route_line(best, line, true) + "." + more;
        }
        if (limit) {
            return "Nothing gets from " + from + " to " + to + " within " + format_minutes(*goal.max_minutes) +
                   ". The quickest way is first. " + route_line(best, line, false) + "." + more;
        }
        return "None of the routes from " + from + " to " + to + " use the modes you asked for, so here is the best of the rest. " +
               route_line(best, line, false) + "." + more;
    }

    switch (goal.objective) {
        case Objective::Cheapest:
            return "Cheapest way from " + from + " to " + to + (budget ? " within your budget" : "") + ". " +
                   route_line(best, line, true) + "." + more;
        case Objective::Fastest:
            return "Fastest way from " + from + " to " + to + (budget ? " within your budget" : "") + ". " +
                   route_line(best, line, false) + "." + more;
        case Objective::FewestChanges:
            return "Fewest changes from " + from + " to " + to + ": " +
                   (best.transfers == 0 ? std::string("no change of train") : best.transfers == 1 ? "one change" : std::to_string(best.transfers) + " changes") +
                   ". " + route_line(best, line, false) + "." + more;
        case Objective::Comfort:
            return "Least walking from " + from + " to " + to + ", " + format_minutes(best.walk_min) + " on foot. " +
                   route_line(best, line, false) + "." + more;
        case Objective::Balanced: break;
    }
    if (budget && limit) {
        return count_of(ranking.feasible, n, ("fit " + format_rupees(*goal.budget_rs) + " and " + format_minutes(*goal.max_minutes)).c_str()) +
               ". The best balance of them is first. " + route_line(best, line, false) + "." + more;
    }
    if (budget) {
        return count_of(ranking.feasible, n, ("fit your " + format_rupees(*goal.budget_rs) + " budget").c_str()) +
               ". The fastest of them is first. " + route_line(best, line, false) + "." + more;
    }
    if (limit) {
        return count_of(ranking.feasible, n, ("get you there within " + format_minutes(*goal.max_minutes)).c_str()) +
               ". The cheapest of them is first. " + route_line(best, line, true) + "." + more;
    }
    return "Best balance of time and money from " + from + " to " + to + ". " + route_line(best, line, false) + "." + more;
}

std::vector<std::string> fit_notes(const TripState& goal, const RouteFacts& route, const Fit& fit) {
    std::vector<std::string> notes;
    if (goal.budget_rs) {
        const double gap = *goal.budget_rs - route.fare_rs;
        if (fit.over_budget_rs > 0) notes.push_back(format_rupees(fit.over_budget_rs) + " over budget");
        else if (gap >= 1) notes.push_back(format_rupees(gap) + " under budget");
        else notes.push_back("on budget");
    }
    if (goal.max_minutes) {
        const double spare = *goal.max_minutes - route.minutes;
        if (fit.over_time_min > 0) notes.push_back(format_minutes(fit.over_time_min) + " too slow");
        else if (spare >= 1) notes.push_back(format_minutes(spare) + " to spare");
        else notes.push_back("just in time");
    }
    const auto& n = goal.needs;
    if (n.metro && !route.metro) notes.push_back("no metro");
    if (n.cab && !route.cab) notes.push_back("no cab");
    if (n.autorickshaw && !route.autorickshaw) notes.push_back("no auto");
    if (n.bike && !route.bike) notes.push_back("no bike taxi");
    return notes;
}

std::vector<std::string> suggestions(const TripState& goal, const std::vector<RouteFacts>& routes) {
    std::vector<std::string> out;
    if (routes.empty()) return out;
    if (goal.objective != Objective::Cheapest) out.push_back("Cheapest way");
    if (goal.objective != Objective::Fastest) out.push_back("Fastest way");
    if (!goal.budget_rs) {
        double cheapest = routes[0].fare_rs, dearest = routes[0].fare_rs;
        for (const auto& r : routes) cheapest = std::min(cheapest, r.fare_rs), dearest = std::max(dearest, r.fare_rs);
        // A round budget between the cheapest and the dearest route, so it actually filters.
        const double budget = std::ceil(std::max(cheapest * 1.5, cheapest + 20.0) / 50.0) * 50.0;
        if (budget < dearest) out.push_back("Under " + format_rupees(budget));
    }
    if (!goal.max_minutes) {
        double quickest = routes[0].minutes, slowest = routes[0].minutes;
        for (const auto& r : routes) quickest = std::min(quickest, r.minutes), slowest = std::max(slowest, r.minutes);
        const double limit = std::ceil(quickest * 1.2 / 15.0) * 15.0;
        if (limit < slowest) {
            const long m = std::lround(limit);
            out.push_back(m < 60 ? "Within " + std::to_string(m) + " minutes"
                                 : "Within " + std::to_string(m / 60) + (m / 60 == 1 ? " hour" : " hours") +
                                       (m % 60 ? " " + std::to_string(m % 60) + " minutes" : ""));
        }
    }
    out.push_back("Way back");
    if (out.size() > 4) out.erase(out.begin() + 3, out.end() - 1);
    return out;
}

std::string smalltalk_reply(Intent intent) {
    switch (intent) {
        case Intent::Greeting:
            return "Hi! Tell me where you want to go, like \"Bennett University to Hauz Khas under ₹200\" or "
                   "\"fastest way from CP to the airport\".";
        case Intent::Help:
            return "Type or say a trip in your own words. I understand where you start and where you're going "
                   "(\"from Noida Sector 18 to India Gate\"), a budget (\"under ₹300\"), a time limit (\"within 2 hours\", "
                   "\"reach by 6 pm\"), when you leave (\"tomorrow at 9 am\"), what matters most (\"cheapest\", \"fastest\", "
                   "\"fewest changes\", \"least walking\") and modes (\"only metro\", \"no cab\"). Hinglish works too: "
                   "\"Bennett se Hauz Khas 200 rupaye mein\".";
        case Intent::Thanks: return "You're welcome! Ask me about another trip any time.";
        case Intent::Trip: break;
    }
    return "Where do you want to go?";
}

}  // namespace safar::assistant
