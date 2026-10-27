#include "fares/adapters.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace safar::fares {

namespace {

std::string coord(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    return buf;
}

double round1(double x) { return std::round(x * 10.0) / 10.0; }
double round2(double x) { return std::round(x * 100.0) / 100.0; }

}  // namespace

bool RateCard::is_night(int hour) const {
    if (night_pct <= 0) return false;
    return night_from <= night_to ? (hour >= night_from && hour < night_to) : (hour >= night_from || hour < night_to);
}

double RateCardProvider::fare(const Ride& ride) const {
    const auto& c = card_;
    const double stuck_min = std::max(0.0, ride.ride_min - ride.free_flow_min);
    const double metered = c.base + c.per_km * std::max(0.0, ride.km - c.base_km) + c.per_min * ride.ride_min +
                           c.waiting_per_min * stuck_min;
    const int hour = std::clamp(ride.hour, 0, 23);
    const double night = c.is_night(hour) ? 1.0 + c.night_pct / 100.0 : 1.0;
    return std::max(c.min_fare, metered) * c.surge[static_cast<std::size_t>(hour)] * night;
}

Quote RateCardProvider::quote(const Ride& ride) const {
    Quote q;
    q.provider = info_.provider;
    q.provider_name = info_.provider_name;
    q.product = info_.product;
    q.product_name = info_.product_name;
    q.mode = info_.mode;
    q.basis = card_.basis;
    const double f = fare(ride);
    q.fare_rs = std::round(f);
    q.fare_low_rs = std::round(f * (1.0 - card_.range_pct));
    q.fare_high_rs = std::round(f * (1.0 + card_.range_pct));
    q.surge = card_.surge[static_cast<std::size_t>(std::clamp(ride.hour, 0, 23))];
    q.pickup_eta_min = card_.pickup_min;
    q.ride_min = ride.ride_min;
    q.distance_km = ride.km;
    q.link = link(ride);
    return q;
}

std::optional<DeepLink> UberProvider::link(const Ride& r) const {
    // https://developer.uber.com/docs/riders/ride-requests/tutorials/deep-links/introduction
    std::string q = "?action=setPickup";
    q += "&pickup[latitude]=" + coord(r.pickup.lat) + "&pickup[longitude]=" + coord(r.pickup.lng);
    if (!r.pickup_name.empty()) q += "&pickup[nickname]=" + url_encode(r.pickup_name);
    q += "&dropoff[latitude]=" + coord(r.drop.lat) + "&dropoff[longitude]=" + coord(r.drop.lng);
    if (!r.drop_name.empty()) q += "&dropoff[nickname]=" + url_encode(r.drop_name);
    return DeepLink{"https://m.uber.com/ul/" + q, "uber://" + q, true, true};
}

std::optional<DeepLink> OlaProvider::link(const Ride& r) const {
    const std::string trip = "&lat=" + coord(r.pickup.lat) + "&lng=" + coord(r.pickup.lng) +
                             "&drop_lat=" + coord(r.drop.lat) + "&drop_lng=" + coord(r.drop.lng);
    std::string web = "https://book.olacabs.com/?serviceType=p2p&category=" + url_encode(info_.app_category) + trip;
    if (!r.pickup_name.empty()) web += "&pickup_name=" + url_encode(r.pickup_name);
    if (!r.drop_name.empty()) web += "&drop_name=" + url_encode(r.drop_name);
    const std::string app = "olacabs://app/launch?landing_page=bk&category=" + url_encode(info_.app_category) + trip;
    return DeepLink{web, app, true, false};
}

std::optional<DeepLink> RapidoProvider::link(const Ride&) const {
    return DeepLink{"https://www.rapido.bike/", "", false, true};
}

std::unique_ptr<RateCardProvider> make_provider(const std::string& adapter, ProductInfo info, RateCard card) {
    if (adapter == "uber") return std::make_unique<UberProvider>(std::move(info), std::move(card));
    if (adapter == "ola") return std::make_unique<OlaProvider>(std::move(info), std::move(card));
    if (adapter == "rapido") return std::make_unique<RapidoProvider>(std::move(info), std::move(card));
    if (adapter == "meter") return std::make_unique<DelhiAutoMeter>(std::move(info), std::move(card));
    throw std::runtime_error("unknown fare adapter '" + adapter + "' (use uber, ola, rapido or meter)");
}

std::string url_encode(const std::string& s) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

nlohmann::json to_json(const Quote& q) {
    nlohmann::json link = nullptr;
    if (q.link) {
        link = {{"url", q.link->url},
                {"app_url", q.link->app_url.empty() ? nlohmann::json(nullptr) : nlohmann::json(q.link->app_url)},
                {"prefilled", q.link->prefilled},
                {"documented", q.link->documented}};
    }
    return {{"provider", q.provider},
            {"provider_name", q.provider_name},
            {"product", q.product},
            {"product_name", q.product_name},
            {"mode", graph::to_string(q.mode)},
            {"fare_rs", q.fare_rs},
            {"fare_range_rs", {q.fare_low_rs, q.fare_high_rs}},
            {"surge", round2(q.surge)},
            {"pickup_eta_min", round1(q.pickup_eta_min)},
            {"ride_min", round1(q.ride_min)},
            {"total_min", round1(q.total_min())},
            {"distance_km", round1(q.distance_km)},
            {"source", q.source},
            {"basis", q.basis},
            {"deep_link", std::move(link)}};
}

}  // namespace safar::fares
