#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>

#include "data/dataset.hpp"
#include "util/geo.hpp"

using namespace safar::data;

namespace {

// Loaded once: these tests check the real committed data, not a fixture.
const Dataset& ds() {
    static const Dataset d = Dataset::load(SAFAR_TEST_DATA_DIR);
    return d;
}

bool has_line(const Station& s, const std::string& line) {
    return std::find(s.lines.begin(), s.lines.end(), line) != s.lines.end();
}

}  // namespace

TEST(Dataset, LoadsAllCoreLines) {
    for (const char* id : {"blue", "yellow", "red", "green", "violet", "pink", "magenta", "airport-express",
                           "grey", "aqua", "rapid-metro-gurugram", "namo-bharat-rrts"}) {
        EXPECT_NE(ds().line(id), nullptr) << id;
    }
    EXPECT_GT(ds().stations().size(), 250u);
    EXPECT_GT(ds().places().size(), 100u);
}

TEST(Dataset, KnownInterchangesServeTheRightLines) {
    const auto* rc = ds().station("rajiv-chowk");
    ASSERT_NE(rc, nullptr);
    EXPECT_TRUE(has_line(*rc, "blue"));
    EXPECT_TRUE(has_line(*rc, "yellow"));

    const auto* kg = ds().station("kashmere-gate");
    ASSERT_NE(kg, nullptr);
    EXPECT_TRUE(has_line(*kg, "yellow"));
    EXPECT_TRUE(has_line(*kg, "red"));
    EXPECT_TRUE(has_line(*kg, "violet"));
}

TEST(Dataset, AquaLineConnectsToBlueLineByWalkLink) {
    // Noida Sector 51 (Aqua) and Noida Sector 52 (Blue) are joined by a walkway.
    const auto& t = ds().transfers();
    auto it = std::find_if(t.begin(), t.end(), [](const Transfer& x) {
        return x.kind == Transfer::Kind::WalkLink &&
               std::set<std::string>{x.a, x.b} == std::set<std::string>{"noida-sector-51", "noida-sector-52"};
    });
    ASSERT_NE(it, t.end());
    EXPECT_LT(it->walk_min, 15);
}

TEST(Dataset, SegmentsArePhysicallyPlausible) {
    for (const auto& s : ds().segments()) {
        EXPECT_GT(s.distance_km, 0.2) << s.a << "-" << s.b;
        EXPECT_LT(s.distance_km, 15.0) << s.a << "-" << s.b;  // RRTS has the longest gaps
        EXPECT_GT(s.time_min, 0.5) << s.a << "-" << s.b;
        // Track distance is never shorter than the straight line between stations.
        const auto* a = ds().station(s.a);
        const auto* b = ds().station(s.b);
        EXPECT_GE(s.distance_km + 1e-6, safar::geo::haversine_km(a->pos, b->pos));
    }
}

TEST(Dataset, EndToEndLineTimesMatchRealityRoughly) {
    std::map<std::string, double> minutes;
    for (const auto& s : ds().segments()) minutes[s.line] += s.time_min;
    EXPECT_NEAR(minutes["yellow"], 85, 10);          // Samaypur Badli -> Millennium City Centre
    EXPECT_NEAR(minutes["airport-express"], 22, 5);  // Dwarka 25 -> New Delhi
    EXPECT_NEAR(minutes["red"], 60, 10);             // Rithala -> Shaheed Sthal
}

TEST(Dataset, PlacesAreInsideNcr) {
    for (const auto& p : ds().places()) {
        EXPECT_GT(p.pos.lat, 28.0) << p.id;
        EXPECT_LT(p.pos.lat, 29.2) << p.id;
        EXPECT_GT(p.pos.lng, 76.7) << p.id;
        EXPECT_LT(p.pos.lng, 77.9) << p.id;
    }
}

TEST(Search, FindsBennettByPrefix) {
    auto hits = ds().search("benn");
    ASSERT_FALSE(hits.empty());
    ASSERT_EQ(hits[0].kind, SearchHit::Kind::Place);
    EXPECT_EQ(hits[0].place->id, "bennett-university");
}

TEST(Search, IsCaseInsensitiveAndMatchesWordStarts) {
    auto hits = ds().search("CHOWK");
    ASSERT_FALSE(hits.empty());
    for (const auto& h : hits) {
        const auto& name = h.place ? h.place->name : h.station->name;
        std::string lc = name;
        std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
        EXPECT_NE(lc.find("chowk"), std::string::npos) << name;
    }
}

TEST(Search, RespectsLimitAndEmptyQuery) {
    EXPECT_LE(ds().search("a", 5).size(), 5u);
    EXPECT_TRUE(ds().search("").empty());
    EXPECT_TRUE(ds().search("zzzzzz").empty());
}
