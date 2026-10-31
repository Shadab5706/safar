#include "assistant/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace safar::assistant {

namespace {

// English and Hinglish number words. "do" (2) is left out: it is far more often the verb.
const std::unordered_map<std::string, double>& number_words() {
    static const std::unordered_map<std::string, double> words = {
        {"zero", 0},      {"one", 1},       {"two", 2},       {"three", 3},     {"four", 4},
        {"five", 5},      {"six", 6},       {"seven", 7},     {"eight", 8},     {"nine", 9},
        {"ten", 10},      {"eleven", 11},   {"twelve", 12},   {"fifteen", 15},  {"twenty", 20},
        {"thirty", 30},   {"forty", 40},    {"fifty", 50},    {"sixty", 60},    {"seventy", 70},
        {"eighty", 80},   {"ninety", 90},   {"ek", 1},        {"teen", 3},      {"char", 4},
        {"chaar", 4},     {"paanch", 5},    {"panch", 5},     {"das", 10},      {"bees", 20},
        {"pachas", 50},   {"dedh", 1.5},    {"dhai", 2.5},    {"dhaai", 2.5},   {"aadha", 0.5},
    };
    return words;
}

bool is_multiplier(const std::string& w, double& factor) {
    if (w == "hundred" || w == "sau") factor = 100;
    else if (w == "thousand" || w == "k" || w == "hazaar" || w == "hazar") factor = 1000;
    else return false;
    return true;
}

// Lower-case ASCII words and numbers; everything else becomes a separator.
std::vector<std::string> split(std::string_view s) {
    std::string clean;
    clean.reserve(s.size() + 8);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x82 &&
            static_cast<unsigned char>(s[i + 2]) == 0xB9) {  // ₹
            clean += " rs ";
            i += 2;
        } else if (std::isalnum(c)) {
            clean += static_cast<char>(std::tolower(c));
        } else if ((c == '.' || c == ':') && !clean.empty() && std::isdigit(static_cast<unsigned char>(clean.back())) &&
                   i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1]))) {
            clean += static_cast<char>(c);  // 1.5, 18:30
        } else if (c == '&') {
            clean += " and ";
        } else if (c == '\'') {
            // humayun's -> humayuns
        } else {
            clean += ' ';
        }
    }

    std::vector<std::string> words;
    std::string cur;
    auto flush = [&] {
        if (!cur.empty()) words.push_back(std::move(cur));
        cur.clear();
    };
    for (std::size_t i = 0; i < clean.size(); ++i) {
        const char c = clean[i];
        if (c == ' ') {
            flush();
            continue;
        }
        // Split where letters meet digits: 300rs, rs300, 6pm, 2h30m.
        if (!cur.empty()) {
            const bool digitish = std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ':';
            const bool prev_digitish = std::isdigit(static_cast<unsigned char>(cur.back())) || cur.back() == '.' || cur.back() == ':';
            if (digitish != prev_digitish) flush();
        }
        cur += c;
    }
    flush();
    return words;
}

}  // namespace

std::vector<Token> tokenize(std::string_view message) {
    std::vector<Token> out;
    for (auto& w : split(message)) {
        Token t{w, std::nullopt, false};
        if (std::isdigit(static_cast<unsigned char>(w[0]))) {
            if (const auto colon = w.find(':'); colon != std::string::npos) {
                const int h = std::atoi(w.substr(0, colon).c_str());
                const int m = std::atoi(w.substr(colon + 1).c_str());
                if (h <= 23 && m <= 59) {
                    t.number = h + m / 60.0;
                    t.clock = true;
                }
            } else {
                t.number = std::strtod(w.c_str(), nullptr);
            }
        } else if (const auto it = number_words().find(w); it != number_words().end()) {
            t.number = it->second;
        }

        // "two hundred", "2k", "do sau" -> one number.
        double factor = 0;
        if (!out.empty() && out.back().number && !out.back().clock && is_multiplier(w, factor)) {
            *out.back().number *= factor;
            out.back().text += " " + w;
            continue;
        }
        if (w == "sau" && !out.empty() && out.back().text == "do") {  // "do sau" = 200
            out.back() = Token{"do sau", 200.0, false};
            continue;
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::string normalize(std::string_view text) {
    std::string out;
    for (const auto& w : split(text)) {
        if (!out.empty()) out += ' ';
        out += w;
    }
    return out;
}

std::size_t edit_distance(std::string_view a, std::string_view b) {
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

double similarity(std::string_view a, std::string_view b) {
    const std::size_t longer = std::max(a.size(), b.size());
    if (longer == 0) return 1.0;
    return 1.0 - static_cast<double>(edit_distance(a, b)) / static_cast<double>(longer);
}

}  // namespace safar::assistant
