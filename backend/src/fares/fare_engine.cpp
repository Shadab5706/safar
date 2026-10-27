#include "fares/fare_engine.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace safar::fares {

using nlohmann::json;

namespace {

graph::RoadMode parse_mode(const std::string& s) {
    for (auto m : graph::kAllRoadModes) {
        if (s == graph::to_string(m)) return m;
    }
    throw std::runtime_error("mode must be cab, auto or bike, not '" + s + "'");
}

double number(const json& j, const char* key, double fallback, double lo, double hi) {
    if (!j.contains(key)) return fallback;
    if (!j.at(key).is_number()) throw std::runtime_error(std::string(key) + " must be a number");
    const double v = j.at(key).get<double>();
    if (!(v >= lo && v <= hi)) {
        throw std::runtime_error(std::string(key) + " must be between " + std::to_string(lo) + " and " +
                                 std::to_string(hi));
    }
    return v;
}

void read_surge(const json& j, std::array<double, 24>& surge) {
    if (!j.contains("surge_by_hour")) return;
    const auto& s = j.at("surge_by_hour");
    if (!s.is_array() || s.size() != 24) throw std::runtime_error("surge_by_hour needs 24 numbers");
    for (std::size_t h = 0; h < 24; ++h) {
        if (!s[h].is_number() || s[h].get<double>() < 0.5 || s[h].get<double>() > 5) {
            throw std::runtime_error("surge_by_hour values must be between 0.5 and 5");
        }
        surge[h] = s[h].get<double>();
    }
}

RateCard read_card(const json& p, const std::array<double, 24>& provider_surge) {
    constexpr double big = 1e6;
    RateCard c;
    c.basis = p.value("basis", std::string("approximate"));
    if (c.basis != "approximate" && c.basis != "fitted" && c.basis != "govt_rate_card") {
        throw std::runtime_error("basis must be approximate, fitted or govt_rate_card");
    }
    c.base = number(p, "base", 0, 0, big);
    c.base_km = number(p, "base_km", 0, 0, 100);
    c.per_km = number(p, "per_km", 0, 0, 1000);
    c.per_min = number(p, "per_min", 0, 0, 100);
    c.min_fare = number(p, "min_fare", 0, 0, big);
    c.waiting_per_min = number(p, "waiting_per_min", 0, 0, 100);
    c.night_pct = number(p, "night_pct", 0, 0, 100);
    c.night_from = static_cast<int>(number(p, "night_from", 23, 0, 23));
    c.night_to = static_cast<int>(number(p, "night_to", 5, 0, 23));
    c.pickup_min = number(p, "pickup_min", 5, 0.5, 120);
    c.range_pct = number(p, "range_pct", 0.15, 0, 0.9);
    if (c.per_km <= 0 && c.per_min <= 0) throw std::runtime_error("per_km or per_min must be positive");
    c.surge = provider_surge;
    read_surge(p, c.surge);  // a product may override its provider's surge curve
    return c;
}

}  // namespace

void sort_quotes(std::vector<Quote>& quotes) {
    std::stable_sort(quotes.begin(), quotes.end(), [](const Quote& a, const Quote& b) {
        if (a.fare_rs != b.fare_rs) return a.fare_rs < b.fare_rs;
        return a.total_min() < b.total_min();
    });
}

FareEngine::FareEngine(std::shared_ptr<const TransportConfig> transport,
                       std::vector<std::unique_ptr<RateCardProvider>> providers)
    : transport_(std::move(transport)), providers_(std::move(providers)) {
    if (!transport_) throw std::invalid_argument("FareEngine needs the transport config");
    for (const auto& p : providers_) by_mode_[static_cast<std::size_t>(p->mode())].push_back(p.get());
}

FareEngine FareEngine::load(const std::filesystem::path& file, std::shared_ptr<const TransportConfig> transport) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot open fare providers config " + file.string());
    try {
        return from_json(json::parse(in, nullptr, true, /*ignore_comments=*/true), std::move(transport));
    } catch (const std::exception& e) {
        throw std::runtime_error(file.string() + ": " + e.what());
    }
}

FareEngine FareEngine::from_json(const json& j, std::shared_ptr<const TransportConfig> transport) {
    if (!j.contains("providers") || !j.at("providers").is_object() || j.at("providers").empty()) {
        throw std::runtime_error("needs a non-empty \"providers\" object");
    }
    std::vector<std::unique_ptr<RateCardProvider>> out;
    for (const auto& [pid, pj] : j.at("providers").items()) {
        if (pid.starts_with('_')) continue;  // "_note" etc.
        const std::string where = "providers." + pid;
        try {
            const auto adapter = pj.at("adapter").get<std::string>();
            std::array<double, 24> surge{};
            surge.fill(1.0);
            read_surge(pj, surge);
            if (!pj.contains("products") || !pj.at("products").is_object() || pj.at("products").empty()) {
                throw std::runtime_error("needs a non-empty \"products\" object");
            }
            for (const auto& [prod, p] : pj.at("products").items()) {
                if (prod.starts_with('_')) continue;
                try {
                    ProductInfo info{pid, pj.at("name").get<std::string>(), prod, p.at("name").get<std::string>(),
                                     parse_mode(p.at("mode").get<std::string>()), p.value("app_category", prod)};
                    RateCard card = read_card(p, surge);
                    if (adapter == "meter" &&
                        std::any_of(card.surge.begin(), card.surge.end(), [](double s) { return s != 1.0; })) {
                        throw std::runtime_error("a government meter has no surge");
                    }
                    out.push_back(make_provider(adapter, std::move(info), std::move(card)));
                } catch (const std::exception& e) {
                    throw std::runtime_error("products." + prod + ": " + e.what());
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(where + ": " + e.what());
        }
    }
    return FareEngine(std::move(transport), std::move(out));
}

Ride FareEngine::ride(graph::RoadMode m, const Trip& t) const {
    const double traffic = transport_->traffic[static_cast<std::size_t>(std::clamp(t.hour, 0, 23))];
    const double ride_min = std::max(1.0, t.free_flow_min * traffic * transport_->road_params(m).speed_factor);
    return {t.pickup, t.drop, t.pickup_name, t.drop_name, t.km, ride_min, ride_min / traffic, t.hour};
}

double FareEngine::search_fare(graph::RoadMode m, const Ride& r) const {
    const auto& products = by_mode_[static_cast<std::size_t>(m)];
    if (products.empty()) return transport_->road_fare(m, r.km, r.ride_min);
    double best = std::numeric_limits<double>::infinity();
    for (const auto* p : products) best = std::min(best, p->fare(r));
    return best;
}

std::vector<Quote> FareEngine::quotes(graph::RoadMode m, const Ride& r) const {
    std::vector<Quote> out;
    const auto& products = by_mode_[static_cast<std::size_t>(m)];
    if (products.empty()) {
        out.push_back(generic_quote(m, r));
        return out;
    }
    for (const auto* p : products) out.push_back(p->quote(r));
    sort_quotes(out);
    return out;
}

std::vector<Quote> FareEngine::quotes(const Trip& t) const {
    std::vector<Quote> out;
    for (auto m : graph::kAllRoadModes) {
        if (t.km > transport_->road_params(m).max_km) continue;
        auto q = quotes(m, ride(m, t));
        out.insert(out.end(), q.begin(), q.end());
    }
    sort_quotes(out);
    return out;
}

const RateCardProvider* FareEngine::find(const std::string& provider, const std::string& product) const {
    for (const auto& p : providers_) {
        if (p->provider() == provider && p->product() == product) return p.get();
    }
    return nullptr;
}

Quote FareEngine::generic_quote(graph::RoadMode m, const Ride& r) const {
    constexpr double kRange = 0.25;  // a generic card is rougher than a provider's
    Quote q;
    q.provider = "generic";
    q.provider_name = "Any";
    q.product = graph::to_string(m);
    q.product_name = q.product;
    q.mode = m;
    q.basis = "generic_rate_card";
    const double f = transport_->road_fare(m, r.km, r.ride_min);
    q.fare_rs = std::round(f);
    q.fare_low_rs = std::round(f * (1 - kRange));
    q.fare_high_rs = std::round(f * (1 + kRange));
    q.pickup_eta_min = transport_->road_params(m).pickup_min;
    q.ride_min = r.ride_min;
    q.distance_km = r.km;
    return q;
}

}  // namespace safar::fares
