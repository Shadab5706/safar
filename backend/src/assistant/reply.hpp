#pragma once

#include <string>
#include <vector>

#include "assistant/assistant.hpp"
#include "assistant/rank.hpp"

namespace safar::assistant {

std::string format_minutes(double minutes);  // "45 min", "1 h", "1 h 5 min" (as the website writes it)
std::string format_rupees(double rupees);    // "₹96"

// "today at 6:00 pm", "tomorrow at 9:00 am", "on Sunday at 10:30 am". Both "YYYY-MM-DDTHH:MM".
std::string describe_departure(const std::string& depart_at, const std::string& now);

// The answer above the routes: what goes first and why, and how the rest are sorted.
std::string trip_reply(const TripState& goal, const std::string& from, const std::string& to,
                       const std::vector<RouteFacts>& routes, const std::vector<std::string>& summaries,
                       const Ranking& ranking);

// Short notes for one route: "₹54 under budget", "25 min too slow", "no metro".
std::vector<std::string> fit_notes(const TripState& goal, const RouteFacts& route, const Fit& fit);

// Follow-ups the traveller can tap, phrased as messages the assistant understands.
std::vector<std::string> suggestions(const TripState& goal, const std::vector<RouteFacts>& routes);

std::string smalltalk_reply(Intent intent);

}  // namespace safar::assistant
