#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fares/adapters.hpp"
#include "fares/provider.hpp"
#include "fares/transport_config.hpp"

namespace safar::fares {

// One road trip before a vehicle type is chosen.
struct Trip {
    geo::LatLng pickup;
    geo::LatLng drop;
    std::string pickup_name;
    std::string drop_name;
    double km = 0;
    double free_flow_min = 0;  // by car on empty roads (OSRM)
    int hour = 9;
    bool from_osrm = false;
};

// Every fare provider from backend/config/providers.json, grouped by vehicle type.
// Thread-safe after construction (read-only).
class FareEngine {
public:
    FareEngine(std::shared_ptr<const TransportConfig> transport, std::vector<std::unique_ptr<RateCardProvider>> providers);

    // Throws with the file name and the bad key if the config is missing or invalid.
    static FareEngine load(const std::filesystem::path& file, std::shared_ptr<const TransportConfig> transport);
    static FareEngine from_json(const nlohmann::json& j, std::shared_ptr<const TransportConfig> transport);

    // Times a trip for one vehicle type: traffic at trip.hour times the mode's speed factor.
    Ride ride(graph::RoadMode m, const Trip& t) const;

    // The one fare the search uses for a road edge: the cheapest product of that vehicle
    // type. A few multiplications per product, so the planner stays fast.
    double search_fare(graph::RoadMode m, const Ride& r) const;

    // Quotes for a ride already timed for one vehicle type, cheapest first.
    std::vector<Quote> quotes(graph::RoadMode m, const Ride& r) const;
    // Quotes from every product of every vehicle type the trip is within range of
    // (the mode's max_km in transport.json), cheapest first.
    std::vector<Quote> quotes(const Trip& t) const;

    const std::vector<std::unique_ptr<RateCardProvider>>& providers() const { return providers_; }
    const RateCardProvider* find(const std::string& provider, const std::string& product) const;

private:
    // A vehicle type with no product configured falls back to transport.json's generic card.
    Quote generic_quote(graph::RoadMode m, const Ride& r) const;

    std::shared_ptr<const TransportConfig> transport_;
    std::vector<std::unique_ptr<RateCardProvider>> providers_;
    std::array<std::vector<const RateCardProvider*>, 3> by_mode_;
};

// Shown next to every list of quotes.
inline constexpr const char* kEstimateNote =
    "Cab, auto and bike fares are estimates from approximate rate cards: Uber, Ola and Rapido have no public "
    "price API. Open a quote's deep link to see the live price.";

// Sorts quotes cheapest first, then quickest.
void sort_quotes(std::vector<Quote>& quotes);

}  // namespace safar::fares
