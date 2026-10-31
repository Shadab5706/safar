#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace safar::assistant {

// One word of a message after normalising: lower case, punctuation dropped, numbers
// split from the letters around them ("₹300" -> "rs" "300", "6pm" -> "6" "pm").
// `number` is set for digits and number words ("two", "dedh" = 1.5, "2k" = 2000).
struct Token {
    std::string text;
    std::optional<double> number;
    bool clock = false;  // "18:30": number = 18.5
};

std::vector<Token> tokenize(std::string_view message);

// The same normalisation for a place or station name, words joined by single spaces.
std::string normalize(std::string_view text);

// Levenshtein distance: insertions, deletions and substitutions all cost 1.
std::size_t edit_distance(std::string_view a, std::string_view b);

// 1 - distance / length of the longer string: 1 = identical, 0 = nothing alike.
double similarity(std::string_view a, std::string_view b);

}  // namespace safar::assistant
