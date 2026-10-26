#include <gtest/gtest.h>

#include "util/geo.hpp"

using safar::geo::haversine_km;
using safar::geo::LatLng;

TEST(Haversine, ZeroForSamePoint) {
    LatLng rajiv_chowk{28.6328, 77.2197};
    EXPECT_NEAR(haversine_km(rajiv_chowk, rajiv_chowk), 0.0, 1e-9);
}

TEST(Haversine, IsSymmetric) {
    LatLng a{28.6328, 77.2197}, b{28.5494, 77.2001};
    EXPECT_DOUBLE_EQ(haversine_km(a, b), haversine_km(b, a));
}

TEST(Haversine, RajivChowkToHauzKhasRoughly9Km) {
    // Straight-line distance between the two metro stations is about 9.5 km.
    LatLng rajiv_chowk{28.6328, 77.2197}, hauz_khas{28.5434, 77.2066};
    EXPECT_NEAR(haversine_km(rajiv_chowk, hauz_khas), 10.0, 1.0);
}
