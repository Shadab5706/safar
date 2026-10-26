#pragma once

#include <cmath>
#include <numbers>

namespace safar::geo {

struct LatLng {
    double lat = 0.0;
    double lng = 0.0;
};

inline constexpr double kEarthRadiusKm = 6371.0088;

// Great-circle distance in km. Used as the base of the A* heuristic: no road or
// metro route can be shorter than the straight line, so h(n) never overestimates.
inline double haversine_km(LatLng a, LatLng b) {
    constexpr double deg = std::numbers::pi / 180.0;
    const double dlat = (b.lat - a.lat) * deg;
    const double dlng = (b.lng - a.lng) * deg;
    const double s = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(a.lat * deg) * std::cos(b.lat * deg) * std::sin(dlng / 2) * std::sin(dlng / 2);
    return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(s));
}

}  // namespace safar::geo
