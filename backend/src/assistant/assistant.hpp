#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "assistant/classifier.hpp"
#include "assistant/text.hpp"
#include "data/dataset.hpp"
#include "journey/planner.hpp"

namespace safar::assistant {

// What the traveller cares about most. Balanced = nothing said, use the planner's score.
enum class Objective { Balanced, Cheapest, Fastest, FewestChanges, Comfort };
enum class Intent { Trip, Greeting, Help, Thanks };

const char* to_string(Objective o);
std::optional<Objective> parse_objective(std::string_view s);

// Modes a route has to use ("by metro", "cab from A to B"), on top of the ones it may use.
struct Needs {
    bool metro = false;
    bool cab = false;
    bool autorickshaw = false;
    bool bike = false;

    bool any() const { return metro || cab || autorickshaw || bike; }
};

// What the conversation has settled so far. The client gets it back as "context"
// and sends it with the next message, so follow-ups ("make it under 200",
// "what if I leave at 6?") refine the same trip.
struct TripState {
    enum class Slot { None, From, To };

    std::optional<journey::Endpoint> from;
    std::optional<journey::Endpoint> to;
    Objective objective = Objective::Balanced;
    std::optional<double> budget_rs;    // most the traveller wants to pay
    std::optional<double> max_minutes;  // longest the trip may take
    std::optional<std::string> depart_at;  // "YYYY-MM-DDTHH:MM"; unset = leave now
    journey::ModeSet modes;                // modes a route may use
    Needs needs;
    Slot awaiting = Slot::None;  // the question we last asked
};

// One place or station name found in a message.
struct PlaceMention {
    enum class Role { Unknown, From, To };

    journey::Endpoint endpoint;
    std::string text;  // the words that matched, normalised
    double score = 0;  // 1 = the exact name
    std::size_t begin = 0, end = 0;  // token range [begin, end)
    bool alias = false;              // matched a nickname ("cp", "gurgaon")
    Role role = Role::Unknown;
};

struct Analysis {
    Intent intent = Intent::Trip;
    std::string label;      // classifier label
    double confidence = 0;  // classifier posterior of that label
    TripState state;        // the previous state updated with this message
    std::vector<PlaceMention> places;
    bool constraints = false;  // a budget, time, departure or mode was mentioned
    bool reset = false;        // "start over"
    std::vector<std::string> notes;  // assumptions worth telling the traveller
};

// Natural-language front end of the planner ("Ask Safar"). Reads a typed or spoken
// request in English or Hinglish and turns it into a trip:
//   1. tokenise and normalise (text.hpp)
//   2. pull out amounts, durations, clock times, days and modes with patterns
//   3. find place names with a gazetteer of every place, station and nickname,
//      tolerant to typos (edit distance) and partial names ("bennett")
//   4. decide which place is the start and which the destination from cue words
//      ("from", "to", Hinglish "se", "tak") and word order
//   5. classify what the traveller wants (cheapest, fastest, ...) with Naive Bayes
//   6. merge with the earlier state of the conversation
// Read-only after construction, so one instance serves every request thread.
class Assistant {
public:
    Assistant(std::shared_ptr<const data::Dataset> ds, const nlohmann::json& config);

    // Reads config/assistant.json. Throws std::runtime_error if it is missing or wrong.
    static Assistant load(const std::filesystem::path& file, std::shared_ptr<const data::Dataset> ds);

    // now = the traveller's local time, "YYYY-MM-DDTHH:MM".
    Analysis analyse(std::string_view message, const TripState& previous, const std::string& now) const;

    // Place names in the tokens not already used by an amount, time or mode.
    std::vector<PlaceMention> find_places(const std::vector<Token>& tokens, const std::vector<char>& used) const;

    const NaiveBayes& classifier() const { return nb_; }
    const data::Dataset& dataset() const { return *ds_; }
    std::size_t gazetteer_size() const { return entries_.size(); }

private:
    struct Entry {
        std::vector<std::string> words;
        std::string joined;   // words with spaces
        std::string compact;  // words without spaces ("hauzkhas")
        journey::Endpoint endpoint;
        bool alias = false;
        bool common = false;  // a one-word name that is also an everyday word ("welcome")
    };

    double score(const std::vector<const Token*>& window, const Entry& e, bool& prefix) const;

    std::shared_ptr<const data::Dataset> ds_;
    std::vector<Entry> entries_;
    NaiveBayes nb_;
};

// "place:<id>" / "station:<id>".
std::string endpoint_id(const data::Dataset& ds, const journey::Endpoint& e);
std::string endpoint_name(const data::Dataset& ds, const journey::Endpoint& e);

// The conversation state as the client keeps it, and back. state_from_json throws
// std::invalid_argument for ids or values it does not know.
nlohmann::json to_json(const TripState& s, const data::Dataset& ds);
TripState state_from_json(const nlohmann::json& j, const data::Dataset& ds);

std::vector<std::string> mode_names(const journey::ModeSet& m);
std::vector<std::string> need_names(const Needs& n);

}  // namespace safar::assistant
