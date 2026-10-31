#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "assistant/assistant.hpp"

namespace safar::assistant {

// The numbers the ranking needs from one planned route.
struct RouteFacts {
    double minutes = 0;
    double fare_rs = 0;
    int transfers = 0;
    double walk_min = 0;
    double score = 0;  // the planner's minutes + rupees / value-of-time (lower is better)
    bool metro = false, cab = false, autorickshaw = false, bike = false;  // modes it uses
};

// From one route of the /journeys JSON.
RouteFacts facts_from_json(const nlohmann::json& route);

enum class SortKey { Score, Price, Time, Changes, Walking };
const char* to_string(SortKey k);

struct Fit {
    bool fits = true;
    double over_budget_rs = 0;  // > 0: costs this much more than the budget
    double over_time_min = 0;   // > 0: takes this much longer than allowed
    bool missing_mode = false;  // does not use a mode that was asked for ("by metro")
};

// Constraint-based ranking. A route is feasible if it meets every constraint: the
// budget, the time limit and the modes asked for. The best feasible route by the
// "pick" key goes first; all the others follow, feasible ones first, sorted by the
// "list" key:
//   budget only        pick = the fastest you can afford, list = cheapest first
//   time limit only    pick = the cheapest that is quick enough, list = quickest first
//   both               pick = best balance, list = cheapest first
//   "cheapest", ...    pick and list both by what was asked for
// If no route is feasible, the one closest to the constraints goes first.
struct Ranking {
    std::vector<std::size_t> order;  // route indices, best match first
    std::vector<Fit> fit;            // per route, in the original order
    SortKey pick = SortKey::Score;
    SortKey list = SortKey::Score;
    std::size_t feasible = 0;
};

Ranking rank(const std::vector<RouteFacts>& routes, const TripState& goal);

}  // namespace safar::assistant
