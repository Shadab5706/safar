#include "roads/road_matrix.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace safar::roads {

RoadMatrix RoadMatrix::load(const std::filesystem::path& file) {
    RoadMatrix m;
    std::ifstream in(file);
    if (!in) return m;
    const auto j = nlohmann::json::parse(in);
    const auto& pairs = j.at("pairs");
    m.pairs_.reserve(pairs.size());
    for (const auto& p : pairs) {
        m.pairs_.emplace(p.at(0).get<std::string>() + "|" + p.at(1).get<std::string>(),
                         std::make_pair(p.at(2).get<float>(), p.at(3).get<float>()));
    }
    return m;
}

std::optional<RoadLeg> RoadMatrix::lookup(const std::string& a, const std::string& b) const {
    auto it = pairs_.find(a + "|" + b);
    if (it == pairs_.end()) it = pairs_.find(b + "|" + a);
    if (it == pairs_.end()) return std::nullopt;
    return RoadLeg{it->second.first, it->second.second, true};
}

}  // namespace safar::roads
