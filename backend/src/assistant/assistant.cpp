#include "assistant/assistant.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace safar::assistant {

using nlohmann::json;
using Words = std::unordered_set<std::string>;

namespace {

// ---- word lists ----

const Words kHourUnits = {"h", "hr", "hrs", "hour", "hours", "ghanta", "ghante", "ghanton"};
const Words kMinuteUnits = {"m", "min", "mins", "minute", "minutes", "minat", "mint", "minit"};
const Words kCurrency = {"rs", "inr", "rupees", "rupee", "rupaye", "rupay", "rupiya", "rupya", "rupaiya", "bucks"};
// A bare number right after these is an amount ("under 300", "budget of 300").
const Words kMoneyBefore = {"under", "below", "within", "max", "maximum", "upto", "budget", "than", "spend",
                            "afford", "limit", "of", "is", "only", "around", "about", "have"};
const Words kMoneyAfter = {"max", "budget", "tak", "mein", "only", "maximum", "limit"};
const Words kArriveBefore = {"by", "before", "reach", "arrive", "arriving", "there", "pahunchna", "pahuchna"};
const Words kLeaveBefore = {"leave", "leaving", "start", "starting", "depart", "departing", "nikalna", "nikal", "after"};
const Words kAm = {"am", "subah", "subha", "morning"};
const Words kPm = {"pm", "shaam", "sham", "evening", "raat", "night", "dopahar", "afternoon"};
const Words kOClock = {"baje", "bje", "oclock"};

const Words kNegation = {"no", "not", "without", "avoid", "except", "dont", "skip", "nahi", "nhi", "bina", "never", "hate", "exclude"};
const Words kOnly = {"only", "just", "sirf", "purely", "exclusively"};

const Words kReset = {"reset", "restart", "clear"};
const Words kSwap = {"back", "return", "reverse", "wapas", "vapas", "swap"};
const Words kNow = {"now", "abhi", "immediately"};

// Cue words around a place name that say whether it is the start or the destination.
const Words kFromBefore = {"from", "frm", "starting", "start", "leaving", "pickup", "origin", "source"};
const Words kToBefore = {"to", "till", "until", "towards", "toward", "reach", "reaching", "into", "destination",
                         "drop", "for", "visit", "upto", "goto"};
const Words kFromAfter = {"se", "sey", "say"};
const Words kToAfter = {"tak", "jana", "jaana", "jaane", "jaun", "jaon", "pahunchna", "pahuchna", "ko"};
const Words kSkippable = {"the", "a", "an", "my"};

// Words that cannot start a place name, and end one only if the name has them ("... railway station").
const Words kFiller = {
    "to", "from", "frm", "the", "a", "an", "in", "at", "by", "for", "of", "and", "or", "i", "im", "me", "my", "we",
    "us", "want", "wanna", "need", "go", "going", "goto", "get", "reach", "reaching", "se", "tak", "jana", "jaana",
    "jaane", "hai", "mujhe", "ko", "mein", "ka", "ki", "ke", "please", "pls", "plz", "how", "what", "which", "way",
    "route", "routes", "best", "is", "it", "can", "could", "do", "take", "show", "find", "near", "via", "then",
    "there", "here", "with", "without", "only", "just", "no", "not", "travel", "trip", "journey", "station", "stn",
    "metro", "under", "within", "below", "budget", "cheapest", "fastest", "cheap", "fast", "quick", "quickest",
    "least", "less", "cost", "price", "time", "than", "max", "maximum", "minimum", "also", "leave", "leaving",
    "start", "starting", "towards", "toward", "till", "until", "into", "drop", "pickup", "pick", "up", "on",
    "kaise", "kya", "batao", "bata", "chahiye", "hun", "hoon", "hu", "around", "about", "some", "any", "all",
    "ways", "options", "option", "give", "tell", "suggest", "plan", "planning", "would", "like", "should", "will",
    "where", "when", "fare", "fares", "rasta", "sasta", "sabse", "jaldi", "hey", "hi", "hello", "thanks"};
// One-word station names that are also everyday words: only taken with "metro"/"station" after them.
const Words kCommonNames = {"welcome", "sarai", "ashram"};
const Words kStationWords = {"metro", "station", "stn"};

bool numeric(const std::string& s) { return !s.empty() && std::isdigit(static_cast<unsigned char>(s[0])); }

std::string digits_of(const std::string& s) {
    std::string d;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) d += c;
    }
    return d;
}

// ---- local date/time ----

struct LocalTime {
    std::chrono::sys_days day;
    int minute = 0;  // minutes after midnight
};

std::optional<LocalTime> parse_local(const std::string& s) {
    using namespace std::chrono;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d", &y, &mo, &d, &h, &mi) != 5) return std::nullopt;
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || h < 0 || h > 23 || mi < 0 || mi > 59) return std::nullopt;
    return LocalTime{sys_days{ymd}, h * 60 + mi};
}

std::string format_local(const LocalTime& t) {
    using namespace std::chrono;
    const year_month_day ymd{t.day};
    char buf[64];  // room for any ints, so GCC -Wformat-truncation stays quiet
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02d:%02d", static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()), t.minute / 60, t.minute % 60);
    return buf;
}

LocalTime add_minutes(LocalTime t, int minutes) {
    int m = t.minute + minutes;
    int days = static_cast<int>(std::floor(m / 1440.0));
    t.day += std::chrono::days{days};
    t.minute = m - days * 1440;
    return t;
}

int minutes_between(const LocalTime& a, const LocalTime& b) {
    return static_cast<int>((b.day - a.day).count()) * 1440 + (b.minute - a.minute);
}

int weekday_of(const std::string& w) {
    static const std::unordered_map<std::string, int> days = {
        {"sunday", 0},   {"sun", 0},     {"ravivar", 0},  {"itwar", 0},  {"itwaar", 0}, {"monday", 1},
        {"mon", 1},      {"somvar", 1},  {"tuesday", 2},  {"tue", 2},    {"tues", 2},   {"wednesday", 3},
        {"wed", 3},      {"thursday", 4}, {"thu", 4},     {"thurs", 4},  {"friday", 5}, {"fri", 5},
        {"saturday", 6}, {"sat", 6},     {"weekend", 6},
    };
    const auto it = days.find(w);
    return it == days.end() ? -1 : it->second;
}

// A clock time in the message, before it is placed on a day.
struct ClockMention {
    int hour = 0;
    int minute = 0;
    bool ampm_known = false;
};

// Puts a clock time on the first matching moment at or after `after`, using the part
// of the day ("evening") when am/pm was not said.
LocalTime resolve_clock(const ClockMention& c, int part_hour, LocalTime after, bool same_day_only) {
    int h = c.hour;
    if (!c.ampm_known && h < 12 && part_hour >= 12) h += 12;
    if (c.ampm_known || h >= 12 || part_hour >= 0) {
        LocalTime t{after.day, h * 60 + c.minute};
        if (!same_day_only && t.minute < after.minute) t.day += std::chrono::days{1};
        return t;
    }
    // "at 9" with no am/pm: the next 9 o'clock, morning or evening.
    for (int day = 0; day < 2; ++day) {
        for (int hh : {h, h + 12}) {
            LocalTime t{after.day + std::chrono::days{day}, hh * 60 + c.minute};
            if (minutes_between(after, t) >= 0) return t;
        }
    }
    return {after.day + std::chrono::days{1}, h * 60 + c.minute};
}

}  // namespace

// ---- names ----

const char* to_string(Objective o) {
    switch (o) {
        case Objective::Balanced: return "balanced";
        case Objective::Cheapest: return "cheapest";
        case Objective::Fastest: return "fastest";
        case Objective::FewestChanges: return "fewest_changes";
        case Objective::Comfort: return "comfort";
    }
    return "balanced";
}

std::optional<Objective> parse_objective(std::string_view s) {
    for (auto o : {Objective::Balanced, Objective::Cheapest, Objective::Fastest, Objective::FewestChanges, Objective::Comfort}) {
        if (s == to_string(o)) return o;
    }
    return std::nullopt;
}

std::string endpoint_id(const data::Dataset& ds, const journey::Endpoint& e) {
    return e.type == journey::Endpoint::Type::Place ? "place:" + ds.places()[e.index].id : "station:" + ds.stations()[e.index].id;
}

std::string endpoint_name(const data::Dataset& ds, const journey::Endpoint& e) {
    return e.type == journey::Endpoint::Type::Place ? ds.places()[e.index].name : ds.stations()[e.index].name;
}

namespace {

std::optional<journey::Endpoint> endpoint_from_id(const data::Dataset& ds, const std::string& s) {
    if (s.rfind("place:", 0) == 0) {
        if (const auto* p = ds.place(s.substr(6))) return journey::Endpoint{journey::Endpoint::Type::Place, static_cast<std::size_t>(p - ds.places().data())};
    } else if (s.rfind("station:", 0) == 0) {
        if (const auto* st = ds.station(s.substr(8))) {
            return journey::Endpoint{journey::Endpoint::Type::Station, static_cast<std::size_t>(st - ds.stations().data())};
        }
    }
    return std::nullopt;
}

}  // namespace

std::vector<std::string> mode_names(const journey::ModeSet& m) {
    std::vector<std::string> out;
    if (m.walk) out.push_back("walk");
    if (m.metro) out.push_back("metro");
    if (m.cab) out.push_back("cab");
    if (m.autorickshaw) out.push_back("auto");
    if (m.bike) out.push_back("bike");
    return out;
}

std::vector<std::string> need_names(const Needs& n) {
    std::vector<std::string> out;
    if (n.metro) out.push_back("metro");
    if (n.cab) out.push_back("cab");
    if (n.autorickshaw) out.push_back("auto");
    if (n.bike) out.push_back("bike");
    return out;
}

json to_json(const TripState& s, const data::Dataset& ds) {
    auto opt = [](const auto& v) -> json { return v ? json(*v) : json(nullptr); };
    return {{"from", s.from ? json(endpoint_id(ds, *s.from)) : json(nullptr)},
            {"to", s.to ? json(endpoint_id(ds, *s.to)) : json(nullptr)},
            {"objective", to_string(s.objective)},
            {"budget_rs", opt(s.budget_rs)},
            {"max_minutes", opt(s.max_minutes)},
            {"depart_at", opt(s.depart_at)},
            {"modes", mode_names(s.modes)},
            {"needs", need_names(s.needs)},
            {"awaiting", s.awaiting == TripState::Slot::From ? json("from") : s.awaiting == TripState::Slot::To ? json("to") : json(nullptr)}};
}

TripState state_from_json(const json& j, const data::Dataset& ds) {
    if (!j.is_object()) throw std::invalid_argument("context must be an object");
    TripState s;
    auto endpoint = [&](const char* key) -> std::optional<journey::Endpoint> {
        if (!j.contains(key) || j[key].is_null()) return std::nullopt;
        if (!j[key].is_string()) throw std::invalid_argument(std::string("context.") + key + " must be an id");
        auto e = endpoint_from_id(ds, j[key].get<std::string>());
        if (!e) throw std::invalid_argument(std::string("context.") + key + " is not a known place or station");
        return e;
    };
    auto number = [&](const char* key, double lo, double hi) -> std::optional<double> {
        if (!j.contains(key) || j[key].is_null()) return std::nullopt;
        if (!j[key].is_number() || j[key].get<double>() < lo || j[key].get<double>() > hi) {
            throw std::invalid_argument(std::string("context.") + key + " is out of range");
        }
        return j[key].get<double>();
    };
    s.from = endpoint("from");
    s.to = endpoint("to");
    if (j.contains("objective")) {
        const auto o = j["objective"].is_string() ? parse_objective(j["objective"].get<std::string>()) : std::nullopt;
        if (!o) throw std::invalid_argument("context.objective is not known");
        s.objective = *o;
    }
    s.budget_rs = number("budget_rs", 1, 100000);
    s.max_minutes = number("max_minutes", 1, 1440);
    if (j.contains("depart_at") && !j["depart_at"].is_null()) {
        if (!j["depart_at"].is_string() || !parse_local(j["depart_at"].get<std::string>())) {
            throw std::invalid_argument("context.depart_at must look like 2026-10-27T09:00");
        }
        s.depart_at = j["depart_at"].get<std::string>();
    }
    auto names = [&](const char* key) -> std::vector<std::string> {
        std::vector<std::string> out;
        if (!j.contains(key)) return out;
        if (!j[key].is_array()) throw std::invalid_argument(std::string("context.") + key + " must be a list");
        for (const auto& x : j[key]) {
            if (!x.is_string()) throw std::invalid_argument(std::string("context.") + key + " must be a list of modes");
            out.push_back(x.get<std::string>());
        }
        return out;
    };
    if (j.contains("modes")) {
        s.modes = {false, false, false, false, false};
        for (const auto& m : names("modes")) {
            if (m == "walk") s.modes.walk = true;
            else if (m == "metro") s.modes.metro = true;
            else if (m == "cab") s.modes.cab = true;
            else if (m == "auto") s.modes.autorickshaw = true;
            else if (m == "bike") s.modes.bike = true;
            else throw std::invalid_argument("context.modes has an unknown mode");
        }
    }
    for (const auto& m : names("needs")) {
        if (m == "metro") s.needs.metro = true;
        else if (m == "cab") s.needs.cab = true;
        else if (m == "auto") s.needs.autorickshaw = true;
        else if (m == "bike") s.needs.bike = true;
        else throw std::invalid_argument("context.needs has an unknown mode");
    }
    if (j.contains("awaiting") && j["awaiting"].is_string()) {
        const auto a = j["awaiting"].get<std::string>();
        s.awaiting = a == "from" ? TripState::Slot::From : a == "to" ? TripState::Slot::To : TripState::Slot::None;
    }
    return s;
}

// ---- construction ----

Assistant::Assistant(std::shared_ptr<const data::Dataset> ds, const json& config) : ds_(std::move(ds)) {
    auto add = [&](const std::string& name, journey::Endpoint e, bool alias) {
        Entry entry;
        entry.joined = normalize(name);
        if (entry.joined.empty()) return;
        for (const auto& t : tokenize(entry.joined)) entry.words.push_back(t.text);
        for (const auto& w : entry.words) entry.compact += w;
        entry.endpoint = e;
        entry.alias = alias;
        entry.common = entry.words.size() == 1 && kCommonNames.count(entry.words[0]) > 0;
        entries_.push_back(entry);
        // "The Great India Place" is also just "Great India Place".
        if (entry.words.size() > 1 && entry.words[0] == "the") {
            Entry bare = entry;
            bare.words.erase(bare.words.begin());
            bare.joined = bare.joined.substr(4);
            bare.compact = bare.compact.substr(3);
            entries_.push_back(std::move(bare));
        }
    };
    for (std::size_t i = 0; i < ds_->places().size(); ++i) add(ds_->places()[i].name, {journey::Endpoint::Type::Place, i}, false);
    for (std::size_t i = 0; i < ds_->stations().size(); ++i) add(ds_->stations()[i].name, {journey::Endpoint::Type::Station, i}, false);

    if (!config.contains("aliases") || !config["aliases"].is_object()) throw std::runtime_error("assistant config needs an 'aliases' object");
    for (const auto& [name, target] : config["aliases"].items()) {
        const auto e = target.is_string() ? endpoint_from_id(*ds_, target.get<std::string>()) : std::nullopt;
        if (!e) throw std::runtime_error("assistant alias '" + name + "' points to an unknown place or station");
        add(name, *e, true);
    }

    if (!config.contains("training") || !config["training"].is_object()) throw std::runtime_error("assistant config needs a 'training' object");
    std::vector<NaiveBayes::Example> examples;
    for (const auto& [label, sentences] : config["training"].items()) {
        if (label != "greeting" && label != "help" && label != "thanks" && !parse_objective(label)) {
            throw std::runtime_error("assistant training label '" + label + "' is not known");
        }
        for (const auto& s : sentences) examples.push_back({s.get<std::string>(), label});
    }
    nb_.train(examples);
}

Assistant Assistant::load(const std::filesystem::path& file, std::shared_ptr<const data::Dataset> ds) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot open assistant config " + file.string());
    json config;
    try {
        config = json::parse(in);
    } catch (const json::exception& e) {
        throw std::runtime_error("assistant config " + file.string() + " is not valid JSON: " + e.what());
    }
    return Assistant(std::move(ds), config);
}

// ---- place names ----

double Assistant::score(const std::vector<const Token*>& w, const Entry& e, bool& prefix) const {
    prefix = false;
    const std::size_t k = w.size(), m = e.words.size();

    // Word by word over the first `count` words. Numbers must match exactly ("sector 18"
    // is not "sector 28"); a one-word window needs 5+ letters before a typo is forgiven.
    auto wordwise = [&](std::size_t count) -> double {
        double num = 0, den = 0;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& a = w[i]->text;
            const auto& b = e.words[i];
            const double len = static_cast<double>(std::max(a.size(), b.size()));
            double s = 1;
            if (a != b) {
                if (numeric(a) || numeric(b)) return 0;
                const std::size_t shortest = std::min(a.size(), b.size());
                if (shortest < 3 || (k == 1 && shortest < 5)) return 0;
                s = similarity(a, b);
                if (s < 0.7) return 0;
            }
            num += s * len;
            den += len;
        }
        return den > 0 ? num / den : 0;
    };

    double best = 0;
    if (k == m) {
        const double s = wordwise(k);
        best = s >= 1.0 ? 1.0 : s * 0.97;
    } else if (k < m) {
        // The start of a longer name: "bennett" for Bennett University.
        const double s = wordwise(k);
        std::size_t len = k - 1;
        for (const auto* t : w) len += t->text.size();
        const bool long_enough = len >= 4 && (k > 1 || (w[0]->text.size() >= 5 && !kFiller.count(w[0]->text)));
        if (s >= 0.85 && long_enough) {
            best = s * (0.8 + 0.15 * static_cast<double>(len) / static_cast<double>(e.joined.size()));
            prefix = true;
        }
    }
    // Spaces typed differently: "hauzkhas", "noida sector18".
    if ((k + 1 == m || m + 1 == k) && best < 0.9) {
        std::string c;
        for (const auto* t : w) c += t->text;
        if (digits_of(c) == digits_of(e.compact)) {
            const double s = similarity(c, e.compact);
            if (s >= 0.9 && s * 0.95 > best) {
                best = s * 0.95;
                prefix = false;
            }
        }
    }
    return best;
}

std::vector<PlaceMention> Assistant::find_places(const std::vector<Token>& tokens, const std::vector<char>& used) const {
    struct Candidate {
        std::size_t begin, end;
        const Entry* entry;
        double score, rank;
    };
    std::vector<Candidate> found;
    const std::size_t n = tokens.size();

    for (std::size_t b = 0; b < n; ++b) {
        if (used[b] || kFiller.count(tokens[b].text) || numeric(tokens[b].text)) continue;
        std::vector<const Token*> window;
        for (std::size_t e = b; e < n && e - b < 6; ++e) {
            if (used[e]) break;
            window.push_back(&tokens[e]);
            const bool station_hint = e + 1 < n && kStationWords.count(tokens[e + 1].text) > 0;
            const bool ends_in_filler = kFiller.count(tokens[e].text) > 0;

            std::vector<Candidate> whole, prefix;
            for (const auto& entry : entries_) {
                bool is_prefix = false;
                const double s = score(window, entry, is_prefix);
                if (s < 0.8) continue;
                if (ends_in_filler && entry.words.back() != tokens[e].text) continue;
                if (entry.common && !station_hint) continue;
                const bool station = entry.endpoint.type == journey::Endpoint::Type::Station;
                const double rank = s + 0.03 * static_cast<double>(window.size()) + (station ? 0.0 : 0.005) +
                                    (station && station_hint ? 0.05 : 0.0);
                (is_prefix ? prefix : whole).push_back({b, e + 1, &entry, s, rank});
            }
            // A partial name only counts if it points at one name ("india" could be India Gate or India Expo Mart).
            std::set<std::string> names;
            for (const auto& c : prefix) names.insert(c.entry->joined);
            if (names.size() == 1) found.insert(found.end(), prefix.begin(), prefix.end());
            found.insert(found.end(), whole.begin(), whole.end());
        }
    }

    std::stable_sort(found.begin(), found.end(), [](const Candidate& x, const Candidate& y) { return x.rank > y.rank; });
    std::vector<char> taken(n, 0);
    std::vector<PlaceMention> out;
    for (const auto& c : found) {
        bool free = true;
        for (std::size_t i = c.begin; i < c.end; ++i) free = free && !taken[i];
        if (!free) continue;
        for (std::size_t i = c.begin; i < c.end; ++i) taken[i] = 1;
        std::string text;
        for (std::size_t i = c.begin; i < c.end; ++i) text += (text.empty() ? "" : " ") + tokens[i].text;
        out.push_back({c.entry->endpoint, text, c.score, c.begin, c.end, c.entry->alias, PlaceMention::Role::Unknown});
    }
    std::sort(out.begin(), out.end(), [](const PlaceMention& x, const PlaceMention& y) { return x.begin < y.begin; });
    return out;
}

// ---- the whole message ----

Analysis Assistant::analyse(std::string_view message, const TripState& previous, const std::string& now_text) const {
    Analysis a;
    const auto tokens = tokenize(message);
    const std::size_t n = tokens.size();
    std::vector<char> used(n, 0);
    std::vector<int> span(n, -1);  // which placeholder span a token belongs to
    std::vector<std::string> span_tag;
    auto word = [&](std::size_t i) -> const std::string& {
        static const std::string none;
        return i < n ? tokens[i].text : none;
    };
    auto in = [&](std::size_t i, const Words& set) { return i < n && set.count(tokens[i].text) > 0; };
    auto before = [&](std::size_t i, std::size_t reach, const Words& set) {
        for (std::size_t k = 1; k <= reach && k <= i; ++k) {
            if (set.count(tokens[i - k].text)) return true;
        }
        return false;
    };
    auto mark = [&](std::size_t b, std::size_t e, const char* tag) {
        const int id = static_cast<int>(span_tag.size());
        span_tag.push_back(tag);
        for (std::size_t i = b; i < e && i < n; ++i) {
            used[i] = 1;
            span[i] = id;
        }
    };
    auto plain_number = [&](std::size_t i) { return i < n && tokens[i].number && !tokens[i].clock && !used[i]; };

    const LocalTime now = parse_local(now_text).value_or(LocalTime{std::chrono::sys_days{std::chrono::year{2026} / 1 / 1}, 9 * 60});

    // 1. Durations: "in 2 hours", "1 hr 30 min", "90 mins", "half an hour", "dedh ghanta".
    std::optional<double> limit_min;
    std::optional<int> leave_in_min;
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) continue;
        double minutes = -1;
        std::size_t j = i;
        if ((word(i) == "an" || word(i) == "a") && in(i + 1, kHourUnits)) {
            minutes = 60, j = i + 2;
        } else if (word(i) == "half" && (word(i + 1) == "an" || word(i + 1) == "a") && in(i + 2, kHourUnits)) {
            minutes = 30, j = i + 3;
        } else if (word(i) == "couple" && word(i + 1) == "of" && in(i + 2, kHourUnits)) {
            minutes = 120, j = i + 3;
        } else if (plain_number(i) && word(i + 1) == "and" && word(i + 2) == "a" && word(i + 3) == "half" && in(i + 4, kHourUnits)) {
            minutes = (*tokens[i].number + 0.5) * 60, j = i + 5;
        } else if (plain_number(i) && in(i + 1, kHourUnits)) {
            minutes = *tokens[i].number * 60, j = i + 2;
        } else if (plain_number(i) && in(i + 1, kMinuteUnits)) {
            minutes = *tokens[i].number, j = i + 2;
        }
        if (minutes < 0) continue;
        // "... and a half", "2 hr 30 min"
        if (word(j) == "and" && word(j + 1) == "a" && word(j + 2) == "half") {
            minutes += 30, j += 3;
        } else {
            std::size_t k = word(j) == "and" ? j + 1 : j;
            if (plain_number(k) && in(k + 1, kMinuteUnits)) minutes += *tokens[k].number, j = k + 2;
        }
        if (minutes <= 0 || minutes > 1440) continue;
        if (before(i, 2, kLeaveBefore)) leave_in_min = static_cast<int>(std::lround(minutes));
        else limit_min = minutes;
        mark(i, j, "xduration");
    }

    // 2. Clock times: "6 pm", "18:30", "6 baje", "at 9", "noon"; and days: "tomorrow", "sunday", "evening".
    std::optional<ClockMention> leave_clock, arrive_clock;
    int day_offset = -1, weekday = -1, part_hour = -1;
    bool leave_now = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) continue;
        const auto& w = word(i);
        ClockMention c;
        std::size_t j = i;
        bool got = false;
        if (w == "noon" || w == "midnight") {
            c = {w == "noon" ? 12 : 0, 0, true}, j = i + 1, got = true;
        } else if (i < n && tokens[i].number && !used[i]) {
            const double v = *tokens[i].number;
            const bool whole_hour = !tokens[i].clock && v == std::floor(v);
            if (tokens[i].clock || (whole_hour && v >= 0 && v <= 23)) {
                c.hour = static_cast<int>(v);
                c.minute = tokens[i].clock ? static_cast<int>(std::lround((v - c.hour) * 60)) : 0;
                j = i + 1;
                if (word(j) == "am" || word(j) == "pm") {
                    if (c.hour >= 1 && c.hour <= 12) {
                        c.hour = (c.hour % 12) + (word(j) == "pm" ? 12 : 0);
                        c.ampm_known = got = true;
                        ++j;
                    }
                } else if ((word(j) == "a" || word(j) == "p") && word(j + 1) == "m" && c.hour >= 1 && c.hour <= 12) {
                    c.hour = (c.hour % 12) + (word(j) == "p" ? 12 : 0);
                    c.ampm_known = got = true;
                    j += 2;
                } else if (in(j, kOClock) || (word(j) == "o" && word(j + 1) == "clock")) {
                    got = true;
                    j += word(j) == "o" ? 2 : 1;
                } else if (tokens[i].clock) {
                    got = true;
                } else if (i > 0 && (word(i - 1) == "at" || word(i - 1) == "by" || word(i - 1) == "before") &&
                           !in(j, kCurrency) && !in(j, kHourUnits) && !in(j, kMinuteUnits)) {
                    got = true;  // "at 9", "by 6"
                }
                if (got && c.hour >= 13) c.ampm_known = true;
            }
        }
        if (got) {
            // Two words back at most: "by metro at 6" leaves at 6, "reach by 6" arrives by 6.
            const bool arrive = before(i, 2, kArriveBefore) || word(j) == "tak";
            (arrive ? arrive_clock : leave_clock) = c;
            mark(i > 0 && (word(i - 1) == "at" || word(i - 1) == "by") ? i - 1 : i, j + (word(j) == "tak" ? 1 : 0), "xclock");
            continue;
        }

        // Days and parts of the day.
        if (w == "day" && word(i + 1) == "after" && word(i + 2) == "tomorrow") {
            day_offset = 2, mark(i, i + 3, "xclock");
        } else if (w == "parso" || w == "parson") {
            day_offset = 2, mark(i, i + 1, "xclock");
        } else if (w == "tomorrow" || w == "tmrw" || w == "tmr" || w == "tomorow" || w == "kal") {
            day_offset = 1, mark(i, i + 1, "xclock");
        } else if (w == "today" || w == "aaj") {
            day_offset = 0, mark(i, i + 1, "xclock");
        } else if (w == "tonight") {
            day_offset = 0, part_hour = 21, mark(i, i + 1, "xclock");
        } else if (const int wd = weekday_of(w); wd >= 0 && (w.size() > 3 || word(i - 1) == "on")) {
            weekday = wd, mark(i, i + 1, "xclock");
        } else if (w == "morning" || w == "subah" || w == "subha") {
            part_hour = 9, mark(i, i + 1, "xclock");
        } else if (w == "afternoon" || w == "dopahar") {
            part_hour = 14, mark(i, i + 1, "xclock");
        } else if (w == "evening" || w == "shaam" || w == "sham") {
            part_hour = 18, mark(i, i + 1, "xclock");
        } else if (w == "night" || w == "raat") {
            part_hour = 21, mark(i, i + 1, "xclock");
        } else if (kNow.count(w)) {
            leave_now = true;
        }
    }

    // 3. Amounts: "₹300", "300 rupees", "under 300", "budget of 300", "do sau".
    std::optional<double> budget;
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) continue;
        if (in(i, kCurrency) && plain_number(i + 1)) {
            budget = *tokens[i + 1].number, mark(i, i + 2, "xmoney");
        } else if (plain_number(i) && in(i + 1, kCurrency)) {
            budget = *tokens[i].number, mark(i, i + 2, "xmoney");
        } else if (plain_number(i) && *tokens[i].number >= 20 && (before(i, 1, kMoneyBefore) || in(i + 1, kMoneyAfter) ||
                                                                  (i >= 2 && word(i - 1) == "to" && word(i - 2) == "up"))) {
            budget = *tokens[i].number, mark(i, i + 1, "xmoney");
        }
    }
    if (budget && (*budget < 1 || *budget > 100000)) budget.reset();

    // 4. Modes: "only metro", "no cab", "by auto", "without walking", "bike taxi", "metro se".
    journey::ModeSet only{false, false, false, false, false};
    journey::ModeSet excluded{false, false, false, false, false};
    Needs needs;
    bool any_only = false, any_mode = false, avoid_walking = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) continue;
        const auto& w = word(i);
        enum { None, Walk, Metro, Cab, Auto, Bike } mode = None;
        std::size_t j = i + 1;
        if (w == "bike" && (word(i + 1) == "taxi" || word(i + 1) == "taxis")) mode = Bike, j = i + 2;
        else if (w == "metro" || w == "train" || w == "subway" || w == "tube" || w == "rrts" || w == "namo") mode = Metro;
        else if (w == "cab" || w == "cabs" || w == "taxi" || w == "car" || w == "uber" || w == "ola" || w == "sedan" || w == "suv") mode = Cab;
        else if (w == "auto" || w == "autos" || w == "rickshaw" || w == "autorickshaw" || w == "tuktuk" || w == "erickshaw") mode = Auto;
        else if (w == "bike" || w == "bikes" || w == "motorbike" || w == "motorcycle" || w == "scooty" || w == "scooter" || w == "rapido") mode = Bike;
        else if (w == "walk" || w == "walking" || w == "foot" || w == "paidal") mode = Walk;
        if (mode == None) continue;
        // "metro station" names a place, not a mode.
        if (mode == Metro && (word(j) == "station" || word(j) == "stn")) continue;
        any_mode = true;
        const bool neg = before(i, 3, kNegation) || in(j, {"nahi", "nhi", "mat"});  // "don't want a cab"
        const bool solely = before(i, 2, kOnly) || word(j) == "only" || word(j) == "hi";
        for (std::size_t k = i; k < j; ++k) used[k] = 1;
        if (mode == Walk) {
            if (neg) avoid_walking = true;
            else if (solely) any_only = true, only.walk = true;
            continue;
        }
        bool* in_only = mode == Metro ? &only.metro : mode == Cab ? &only.cab : mode == Auto ? &only.autorickshaw : &only.bike;
        bool* in_excl = mode == Metro ? &excluded.metro : mode == Cab ? &excluded.cab : mode == Auto ? &excluded.autorickshaw : &excluded.bike;
        bool* in_need = mode == Metro ? &needs.metro : mode == Cab ? &needs.cab : mode == Auto ? &needs.autorickshaw : &needs.bike;
        if (neg) *in_excl = true;
        else if (solely) any_only = true, *in_only = true;
        else *in_need = true;  // "by metro", "cab from A to B": the route should use it
    }

    // 5. Place names, then who is the start and who the destination.
    a.places = find_places(tokens, used);
    for (auto& p : a.places) {
        std::size_t k = p.begin;
        while (k > 0 && kSkippable.count(tokens[k - 1].text)) --k;
        if (k > 0 && kFromBefore.count(tokens[k - 1].text)) p.role = PlaceMention::Role::From;
        else if (k > 0 && kToBefore.count(tokens[k - 1].text)) p.role = PlaceMention::Role::To;
        else if (in(p.end, kFromAfter)) p.role = PlaceMention::Role::From;
        else if (in(p.end, kToAfter)) p.role = PlaceMention::Role::To;
    }
    // Two places and "A to B": A is the start.
    for (std::size_t i = 0; i + 1 < a.places.size(); ++i) {
        if (a.places[i].role == PlaceMention::Role::Unknown && word(a.places[i].end) == "to") a.places[i].role = PlaceMention::Role::From;
    }
    std::optional<journey::Endpoint> from, to;
    std::vector<const PlaceMention*> unknown;
    for (const auto& p : a.places) {
        if (p.role == PlaceMention::Role::From && !from) from = p.endpoint;
        else if (p.role == PlaceMention::Role::To) to = p.endpoint;
        else if (p.role == PlaceMention::Role::Unknown) unknown.push_back(&p);
    }
    for (const auto* p : unknown) {
        if (!from && (to || unknown.size() > 1 || previous.awaiting == TripState::Slot::From)) {
            from = p->endpoint;
        } else if (!to) {
            to = p->endpoint;
        }
    }
    for (auto& p : a.places) {
        mark(p.begin, p.end, "xplace");
        if ((p.score < 1.0 || p.alias) && p.text.size() >= 4 && p.text != normalize(endpoint_name(*ds_, p.endpoint))) {
            a.notes.push_back("Took \"" + p.text + "\" as " + endpoint_name(*ds_, p.endpoint) + ".");
        }
    }

    // 6. What does the traveller want? Classify the message with the details replaced by placeholders.
    std::vector<std::string> words;
    for (std::size_t i = 0; i < n; ++i) {
        if (span[i] >= 0) {
            if (i == 0 || span[i - 1] != span[i]) words.push_back(span_tag[static_cast<std::size_t>(span[i])]);
        } else {
            words.push_back(tokens[i].text);
        }
    }
    const auto guess = nb_.predict(words);
    a.label = guess.label;
    a.confidence = guess.probability;

    a.reset = std::any_of(tokens.begin(), tokens.end(), [](const Token& t) { return kReset.count(t.text) > 0; }) ||
              (word(0) == "start" && word(1) == "over") || (word(0) == "new" && word(1) == "trip");
    a.constraints = budget || limit_min || leave_in_min || leave_clock || arrive_clock || day_offset >= 0 || weekday >= 0 ||
                    part_hour >= 0 || leave_now || any_mode || avoid_walking;

    const bool smalltalk = a.label == "greeting" || a.label == "help" || a.label == "thanks";
    if (smalltalk && a.places.empty() && !a.constraints) {
        a.intent = a.label == "greeting" ? Intent::Greeting : a.label == "help" ? Intent::Help : Intent::Thanks;
        a.state = previous;
        return a;
    }

    // 7. Merge with what the conversation already knows.
    const bool new_trip = (from && to) || a.reset;
    TripState s = new_trip ? TripState{} : previous;
    s.awaiting = TripState::Slot::None;
    if (from) s.from = from;
    if (to) s.to = to;
    if (!from && !to && a.places.empty() && std::any_of(tokens.begin(), tokens.end(), [](const Token& t) { return kSwap.count(t.text) > 0; })) {
        std::swap(s.from, s.to);
    }

    if (budget) s.budget_rs = budget;
    if (limit_min) s.max_minutes = limit_min;

    const auto objective = parse_objective(a.label);
    if (objective && *objective != Objective::Balanced && a.confidence >= 0.4) s.objective = *objective;
    if (avoid_walking) s.objective = Objective::Comfort;

    if (any_mode) {
        journey::ModeSet m;  // all
        if (any_only) m = {true, only.metro, only.cab, only.autorickshaw, only.bike};
        if (excluded.metro) m.metro = false;
        if (excluded.cab) m.cab = false;
        if (excluded.autorickshaw) m.autorickshaw = false;
        if (excluded.bike) m.bike = false;
        if (!m.metro && !m.any_road()) m = journey::ModeSet{};  // "no metro no cab no auto no bike": ignore
        s.modes = m;
        s.needs = needs;
        if (s.needs.metro && !s.modes.metro) s.needs.metro = false;
    }

    // Departure and arrival times.
    if (leave_now) s.depart_at.reset();
    const bool timed = leave_in_min || leave_clock || day_offset >= 0 || weekday >= 0 || part_hour >= 0;
    LocalTime depart = s.depart_at ? parse_local(*s.depart_at).value_or(now) : now;
    if (timed) {
        if (leave_in_min) {
            depart = add_minutes(now, *leave_in_min);
        } else {
            LocalTime base = now;
            int offset = day_offset;
            if (weekday >= 0) {
                const int today = static_cast<int>(std::chrono::weekday{now.day}.c_encoding());
                offset = (weekday - today + 7) % 7;
            }
            if (offset > 0) base = {now.day + std::chrono::days{offset}, 0};
            if (leave_clock) {
                depart = resolve_clock(*leave_clock, part_hour, base, offset > 0);
            } else if (part_hour >= 0) {
                depart = {base.day, part_hour * 60};
                if (offset <= 0 && depart.minute < now.minute && part_hour != 21) depart.day += std::chrono::days{1};
            } else {
                depart = offset > 0 ? LocalTime{base.day, 9 * 60} : now;
            }
        }
        s.depart_at = format_local(depart);
    }
    if (arrive_clock) {
        const LocalTime arrive = resolve_clock(*arrive_clock, part_hour, depart, false);
        const int minutes = minutes_between(depart, arrive);
        if (minutes > 0 && minutes <= 720) s.max_minutes = minutes;
        else a.notes.push_back("That arrival time is too far from the departure, so I ignored it.");
    }

    // Ask for what is still missing next time round.
    if (!s.from && s.to) s.awaiting = TripState::Slot::From;
    else if (s.from && !s.to) s.awaiting = TripState::Slot::To;
    a.intent = Intent::Trip;
    a.state = std::move(s);
    return a;
}

}  // namespace safar::assistant
