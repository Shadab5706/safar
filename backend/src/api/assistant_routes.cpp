// Ask Safar: plan a trip from a typed or spoken request.
//
// POST /api/v1/assistant
// {
//   "message": "bennett to hauz khas under 300",
//   "context": {...},              optional, the "context" of the previous reply (follow-ups)
//   "now": "2026-10-31T09:15"      optional, the traveller's local time; default server time
// }
// -> {
//   "reply": "4 of 6 routes fit your ₹300 budget. ...",
//   "kind": "routes" | "ask_from" | "ask_to" | "ask_trip" | "same_place" | "no_route" | "smalltalk" | "not_understood",
//   "understood": { "from", "to", "objective", "confidence", "budget_rs", "max_minutes", "depart_at", "modes", "needs", "notes" },
//   "context": {...},              send back with the next message
//   "journey": {...} | null,       same as POST /journeys, routes best match first, each with "match"
//   "ranking": {"pick", "list", "feasible"} | null,
//   "suggestions": ["Cheapest way", "Under ₹200", ...]
// }

#include <string>

#include "api/params.hpp"
#include "api/server.hpp"
#include "assistant/rank.hpp"
#include "assistant/reply.hpp"

namespace safar::api {

using nlohmann::json;
namespace as = assistant;

namespace {

json understood_json(const as::Analysis& a, const data::Dataset& ds) {
    const auto& s = a.state;
    auto opt = [](const auto& v) -> json { return v ? json(*v) : json(nullptr); };
    return {{"from", s.from ? endpoint_ref(ds, *s.from) : json(nullptr)},
            {"to", s.to ? endpoint_ref(ds, *s.to) : json(nullptr)},
            {"objective", as::to_string(s.objective)},
            {"label", a.label},
            {"confidence", std::round(a.confidence * 100) / 100},
            {"budget_rs", opt(s.budget_rs)},
            {"max_minutes", opt(s.max_minutes)},
            {"depart_at", opt(s.depart_at)},
            {"modes", as::mode_names(s.modes)},
            {"needs", as::need_names(s.needs)},
            {"notes", a.notes}};
}

double preference_for(as::Objective o) {
    switch (o) {
        case as::Objective::Cheapest: return 0.15;
        case as::Objective::Fastest: return 0.85;
        default: return 0.5;
    }
}

}  // namespace

void Server::register_assistant_routes() {
    http_.Post("/api/v1/assistant", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const json body = json::parse(req.body);  // bad JSON -> 400 via the exception handler
        try {
            if (!body.is_object()) throw BadRequest{400, "invalid_body", "body must be a JSON object"};
            if (!body.contains("message") || !body["message"].is_string()) {
                throw BadRequest{400, "invalid_field", "message is required"};
            }
            const auto message = body["message"].get<std::string>();
            if (message.find_first_not_of(" \t\r\n") == std::string::npos) throw BadRequest{400, "invalid_field", "message is empty"};
            if (message.size() > 500) throw BadRequest{400, "invalid_field", "message must be at most 500 characters"};

            const auto& ds = *ctx_.dataset;
            as::TripState previous;
            if (body.contains("context") && !body["context"].is_null()) {
                try {
                    previous = as::state_from_json(body["context"], ds);
                } catch (const std::invalid_argument& e) {
                    throw BadRequest{400, "invalid_context", e.what()};
                }
            }
            std::string now_text;
            if (body.contains("now")) {
                if (!body["now"].is_string()) throw BadRequest{400, "invalid_field", "now must look like 2026-10-27T09:00"};
                now_text = body["now"].get<std::string>();
            }
            const auto now = parse_depart(body.contains("now") ? &now_text : nullptr, "now");

            const auto a = ctx_.assistant->analyse(message, previous, now.at);
            const auto& s = a.state;
            json data = {{"understood", understood_json(a, ds)}, {"context", as::to_json(s, ds)}, {"journey", nullptr},
                         {"ranking", nullptr}, {"suggestions", json::array()}};
            auto answer = [&](const char* kind, const std::string& reply) {
                data["kind"] = kind;
                data["reply"] = reply;
                send_ok(res, t0, data);
            };

            if (a.intent != as::Intent::Trip) return answer("smalltalk", as::smalltalk_reply(a.intent));
            if (!s.from && !s.to) {
                if (a.constraints) return answer("ask_trip", "Got it. Where are you starting from, and where do you want to go?");
                return answer("not_understood",
                              "I couldn't find a place in that. Try something like \"Rajiv Chowk to Cyber Hub under ₹150\" "
                              "or \"fastest way from Noida Sector 18 to India Gate\".");
            }
            if (!s.from) return answer("ask_from", "Going to " + as::endpoint_name(ds, *s.to) + ". Where are you starting from?");
            if (!s.to) return answer("ask_to", "Starting from " + as::endpoint_name(ds, *s.from) + ". Where do you want to go?");
            if (*s.from == *s.to) return answer("same_place", "That's the same place for the start and the destination. Where do you want to go?");

            journey::JourneyRequest jr;
            jr.from = *s.from;
            jr.to = *s.to;
            const auto depart = parse_depart(s.depart_at ? &*s.depart_at : &now.at, "depart_at");
            jr.hour = depart.hour;
            jr.sunday = depart.sunday;
            jr.modes = s.modes;
            jr.preference = preference_for(s.objective);
            jr.max_results = 8;
            const auto closures = disruptions_.snapshot();
            jr.closures = closures.get();

            journey::JourneyResult result;
            try {
                result = ctx_.planner->plan(jr);
            } catch (const journey::PlanError& e) {
                if (e.code != "no_route") throw;
                return answer("no_route", "I couldn't find a route with those modes. Try allowing the metro or a cab, auto or bike taxi.");
            }

            json journey = journey::to_json(result, ds);
            journey["closures"] = closures->closures.size();
            journey["depart"] = {{"at", depart.at}, {"hour", jr.hour}, {"sunday_fares", jr.sunday}};
            journey["fare_note"] = fares::kFairPriceNote;

            std::vector<as::RouteFacts> facts;
            std::vector<std::string> summaries;
            for (const auto& r : journey["routes"]) {
                facts.push_back(as::facts_from_json(r));
                summaries.push_back(r.value("summary", std::string()));
            }
            const auto ranking = as::rank(facts, s);
            const bool constrained = s.budget_rs || s.max_minutes || s.needs.any();
            json ordered = json::array();
            for (std::size_t pos = 0; pos < ranking.order.size(); ++pos) {
                const auto i = ranking.order[pos];
                json r = journey["routes"][i];
                r["rank"] = pos + 1;
                r["match"] = {{"best", pos == 0 && (!constrained || ranking.feasible > 0)},
                              {"fits", ranking.fit[i].fits},
                              {"notes", as::fit_notes(s, facts[i], ranking.fit[i])}};
                ordered.push_back(std::move(r));
            }
            journey["routes"] = std::move(ordered);

            std::string reply = as::trip_reply(s, as::endpoint_name(ds, *s.from), as::endpoint_name(ds, *s.to), facts, summaries, ranking);
            if (s.depart_at) reply = "Leaving " + as::describe_departure(*s.depart_at, now.at) + ". " + reply;
            data["journey"] = std::move(journey);
            data["ranking"] = {{"pick", as::to_string(ranking.pick)}, {"list", as::to_string(ranking.list)}, {"feasible", ranking.feasible}};
            data["suggestions"] = as::suggestions(s, facts);
            answer("routes", reply);
        } catch (const BadRequest& e) {
            send_error(res, e.status, e.code, e.message);
        } catch (const journey::PlanError& e) {
            send_error(res, e.status, e.code, e.what());
        }
    });
}

}  // namespace safar::api
