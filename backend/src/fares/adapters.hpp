#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "fares/provider.hpp"

namespace safar::fares {

// Rate card of one product, from backend/config/providers.json.
//   metered = base + per_km * max(0, km - base_km) + per_min * ride_min
//             + waiting_per_min * (minutes stuck in traffic)
//   fare    = max(min_fare, metered) * surge[hour] * (1 + night_pct / 100 at night)
struct RateCard {
    std::string basis = "approximate";  // approximate | fitted | govt_rate_card
    double base = 0, base_km = 0, per_km = 0, per_min = 0, min_fare = 0;
    double waiting_per_min = 0;
    double night_pct = 0;
    int night_from = 23, night_to = 5;  // night window [from, to), wraps past midnight
    double pickup_min = 5;              // average wait for the vehicle to arrive
    double range_pct = 0.15;            // fare range shown is fare * (1 -/+ range_pct)
    std::array<double, 24> surge{};     // multiplier by hour of day

    RateCard() { surge.fill(1.0); }
    bool is_night(int hour) const;
};

struct ProductInfo {
    std::string provider;  // "uber"
    std::string provider_name;
    std::string product;  // "go"
    std::string product_name;
    graph::RoadMode mode = graph::RoadMode::Cab;
    std::string app_category;  // the provider's own name for the product in its links ("mini")
};

// Prices a ride from a rate card. The provider adapters below only add their deep links;
// an adapter that ever gets real API access overrides quote() and returns source "live".
class RateCardProvider : public FareProvider {
public:
    RateCardProvider(ProductInfo info, RateCard card) : info_(std::move(info)), card_(std::move(card)) {}

    const std::string& provider() const override { return info_.provider; }
    const std::string& product() const override { return info_.product; }
    graph::RoadMode mode() const override { return info_.mode; }
    double fare(const Ride& ride) const override;
    Quote quote(const Ride& ride) const override;

    const ProductInfo& info() const { return info_; }
    const RateCard& card() const { return card_; }

protected:
    virtual std::optional<DeepLink> link(const Ride&) const { return std::nullopt; }

    ProductInfo info_;
    RateCard card_;
};

// Uber universal link (documented by Uber): opens the app with pickup and drop set.
class UberProvider final : public RateCardProvider {
public:
    using RateCardProvider::RateCardProvider;

protected:
    std::optional<DeepLink> link(const Ride& ride) const override;
};

// Ola web booking + app link with pickup, drop and category. Ola no longer documents
// this format publicly, so the link is marked undocumented.
class OlaProvider final : public RateCardProvider {
public:
    using RateCardProvider::RateCardProvider;

protected:
    std::optional<DeepLink> link(const Ride& ride) const override;
};

// Rapido has no public link format that pre-fills a trip, so this opens Rapido and the
// user types the destination.
class RapidoProvider final : public RateCardProvider {
public:
    using RateCardProvider::RateCardProvider;

protected:
    std::optional<DeepLink> link(const Ride& ride) const override;
};

// Delhi government auto meter: official rate card, night surcharge, waiting charge,
// no surge and nothing to book (you hail it on the street), so no link.
class DelhiAutoMeter final : public RateCardProvider {
public:
    using RateCardProvider::RateCardProvider;
};

// adapter: "uber" | "ola" | "rapido" | "meter". Throws on anything else.
std::unique_ptr<RateCardProvider> make_provider(const std::string& adapter, ProductInfo info, RateCard card);

// Percent-encodes everything except unreserved characters (RFC 3986).
std::string url_encode(const std::string& s);

}  // namespace safar::fares
