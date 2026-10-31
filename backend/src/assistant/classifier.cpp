#include "assistant/classifier.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

#include "assistant/text.hpp"

namespace safar::assistant {

namespace {

// Words every kind of request uses ("from <place> to <place>"): they only add noise.
const std::unordered_set<std::string> kStopWords = {
    "a", "an", "the", "to", "from", "i", "im", "me", "my", "is", "am", "are", "it", "of", "for", "and", "or",
    "se", "ko", "hai", "mujhe", "want", "wanna", "go", "going", "get", "need", "on", "at", "by", "in", "way",
    "route", "please", "pls", "plz", "xplace", "xmoney", "xduration", "xclock"};

// Crude suffix stripping so "changing", "changes" and "change" share one feature.
std::string stem(std::string w) {
    auto ends = [&](const char* s) {
        const std::size_t n = std::char_traits<char>::length(s);
        return w.size() > n && w.compare(w.size() - n, n, s) == 0;
    };
    if (w.size() > 5 && ends("ing")) w.resize(w.size() - 3);
    else if (w.size() > 4 && ends("est")) w.resize(w.size() - 3);
    else if (w.size() > 4 && ends("er")) w.resize(w.size() - 2);
    else if (w.size() > 3 && ends("es")) w.resize(w.size() - 2);
    else if (w.size() > 3 && ends("ly")) w.resize(w.size() - 2);
    else if (w.size() > 4 && ends("ed")) w.resize(w.size() - 2);
    else if (w.size() > 3 && ends("s") && !ends("ss")) w.resize(w.size() - 1);
    else if (w.size() > 4 && ends("e")) w.resize(w.size() - 1);
    return w;
}

}  // namespace

std::vector<std::string> NaiveBayes::features(const std::vector<std::string>& words) {
    std::vector<std::string> kept;
    for (const auto& w : words) {
        if (!kStopWords.count(w)) kept.push_back(stem(w));
    }
    // Each feature once per sentence: in short messages, presence matters more than repeats.
    std::vector<std::string> f;
    std::unordered_set<std::string> seen;
    auto add = [&](std::string x) {
        if (seen.insert(x).second) f.push_back(std::move(x));
    };
    for (const auto& w : kept) add(w);
    for (std::size_t i = 0; i + 1 < kept.size(); ++i) add(kept[i] + "_" + kept[i + 1]);
    return f;
}

void NaiveBayes::train(const std::vector<Example>& examples) {
    if (examples.empty()) throw std::runtime_error("the classifier needs at least one training sentence");
    labels_.clear();
    stats_.clear();
    vocabulary_.clear();
    documents_ = examples.size();

    for (const auto& ex : examples) {
        auto it = std::find(labels_.begin(), labels_.end(), ex.label);
        std::size_t li = static_cast<std::size_t>(it - labels_.begin());
        if (it == labels_.end()) {
            labels_.push_back(ex.label);
            stats_.emplace_back();
        }
        auto& s = stats_[li];
        ++s.documents;
        std::vector<std::string> words;
        for (const auto& t : tokenize(ex.text)) words.push_back(t.text);
        for (const auto& f : features(words)) {
            ++s.counts[f];
            ++s.total;
            ++vocabulary_[f];
        }
    }
}

NaiveBayes::Prediction NaiveBayes::predict(const std::vector<std::string>& words) const {
    if (labels_.empty()) throw std::logic_error("NaiveBayes::predict called before train");
    const auto f = features(words);
    const double v = static_cast<double>(vocabulary_.size());

    std::vector<double> log_post(labels_.size());
    for (std::size_t li = 0; li < labels_.size(); ++li) {
        const auto& s = stats_[li];
        double lp = std::log(static_cast<double>(s.documents) / static_cast<double>(documents_));
        for (const auto& x : f) {
            if (!vocabulary_.count(x)) continue;  // never seen in training: says nothing about the label
            const auto it = s.counts.find(x);
            const double c = it == s.counts.end() ? 0.0 : static_cast<double>(it->second);
            lp += std::log((c + 1.0) / (static_cast<double>(s.total) + v));
        }
        log_post[li] = lp;
    }

    // Normalise with log-sum-exp so the winner's posterior is a real probability.
    const auto best = static_cast<std::size_t>(std::max_element(log_post.begin(), log_post.end()) - log_post.begin());
    double sum = 0;
    for (double lp : log_post) sum += std::exp(lp - log_post[best]);
    return {labels_[best], 1.0 / sum};
}

}  // namespace safar::assistant
