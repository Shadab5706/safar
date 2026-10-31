#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace safar::assistant {

// Multinomial Naive Bayes text classifier over stemmed word unigrams and bigrams
// (stop words dropped, each feature counted once per sentence), with
// add-one (Laplace) smoothing:
//   P(label | words) ∝ P(label) · Π P(feature | label)
//   P(feature | label) = (count(feature, label) + 1) / (total(label) + |vocabulary|)
// It learns which words signal "cheapest", "fastest", a greeting, ... from the
// labelled sentences in config/assistant.json, so phrasings it has never seen
// ("I'm broke", "running late") still land on the right label.
class NaiveBayes {
public:
    struct Example {
        std::string text;
        std::string label;
    };

    struct Prediction {
        std::string label;
        double probability = 0;  // posterior of the winning label, 0-1
    };

    void train(const std::vector<Example>& examples);

    // words = the message already tokenised (see features()).
    Prediction predict(const std::vector<std::string>& words) const;

    const std::vector<std::string>& labels() const { return labels_; }
    std::size_t vocabulary_size() const { return vocabulary_.size(); }

    // Unigrams plus "a_b" bigrams of the words.
    static std::vector<std::string> features(const std::vector<std::string>& words);

private:
    struct LabelStats {
        std::size_t documents = 0;
        std::size_t total = 0;  // feature occurrences
        std::unordered_map<std::string, std::size_t> counts;
    };

    std::vector<std::string> labels_;
    std::vector<LabelStats> stats_;
    std::unordered_map<std::string, std::size_t> vocabulary_;
    std::size_t documents_ = 0;
};

}  // namespace safar::assistant
