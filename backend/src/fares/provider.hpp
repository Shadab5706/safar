#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "graph/graph.hpp"
#include "util/geo.hpp"

namespace safar::fares {

// One road ride, already timed for the vehicle type being priced.
struct Ride {
    geo::LatLng pickup;
    geo::LatLng drop;
    std::string pickup_name;  // public place / station names, only used to label deep links
    std::string drop_name;
    double km = 0;
    double ride_min = 0;       // expected in-vehicle time at this hour
    double free_flow_min = 0;  // the same ride on empty roads (the difference is time stuck in traffic)
    int hour = 9;              // 0-23, drives surge and night charges
};

// "Check the live price" link. Only public coordinates and place names go in it.
struct DeepLink {
    std::string url;      // https link: opens the app if installed, else the website
    std::string app_url;  // app scheme link, empty if none
    bool prefilled = false;   // the link carries pickup and drop
    bool documented = false;  // the provider documents this link format publicly
};

struct Quote {
    std::string provider;  // "uber", "ola", "rapido", "delhi_auto", "generic"
    std::string provider_name;
    std::string product;  // "go", "mini", "bike", "meter", ...
    std::string product_name;
    graph::RoadMode mode = graph::RoadMode::Cab;
    // "estimate" for everything today. "live" is reserved for an adapter that gets
    // real API access; nothing else has to change when one does.
    std::string source = "estimate";
    // Where the rate card came from: approximate | fitted | govt_rate_card | generic_rate_card
    std::string basis;
    double fare_rs = 0;
    double fare_low_rs = 0;
    double fare_high_rs = 0;
    double surge = 1;  // time-of-day multiplier already included in the fares
    double pickup_eta_min = 0;
    double ride_min = 0;
    double distance_km = 0;
    std::optional<DeepLink> link;

    double total_min() const { return pickup_eta_min + ride_min; }
};

nlohmann::json to_json(const Quote& q);

// One bookable product (Uber Go, Rapido Bike, the Delhi auto meter, ...).
// Adapters must be thread-safe: quote() is called from many request threads.
class FareProvider {
public:
    virtual ~FareProvider() = default;

    virtual const std::string& provider() const = 0;
    virtual const std::string& product() const = 0;
    virtual graph::RoadMode mode() const = 0;

    // Fare in rupees for the ride, surge and night charges included. Called for every
    // road edge the planner builds, so it must be cheap (no I/O).
    virtual double fare(const Ride& ride) const = 0;

    // The full quote shown to the user: fare range, pickup ETA, deep link.
    virtual Quote quote(const Ride& ride) const = 0;
};

}  // namespace safar::fares
