#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "app/context.hpp"
#include "fares/fare_engine.hpp"

using namespace safar;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::shared_ptr<const fares::TransportConfig> transport() {
    static const auto t = std::make_shared<const fares::TransportConfig>(
        fares::TransportConfig::load(SAFAR_TEST_CONFIG_DIR "/transport.json"));
    return t;
}

const fares::FareEngine& engine() {
    static const auto e = fares::FareEngine::load(SAFAR_TEST_CONFIG_DIR "/providers.json", transport());
    return e;
}

// A providers.json with a single product "test/x".
json one_product(const std::string& adapter, json product, json surge = nullptr) {
    json p = {{"name", "Test"}, {"adapter", adapter}, {"products", {{"x", std::move(product)}}}};
    if (!surge.is_null()) p["surge_by_hour"] = std::move(surge);
    return {{"providers", {{"test", std::move(p)}}}};
}

json cab_card() {
    return {{"name", "Test Cab"}, {"mode", "cab"}, {"base", 50}, {"per_km", 12}, {"per_min", 1.5}, {"min_fare", 85}};
}

fares::Ride ride(double km, double ride_min, double free_min, int hour) {
    return {{28.45, 77.58}, {28.55, 77.20}, "Bennett University", "Hauz Khas", km, ride_min, free_min, hour};
}

fares::Trip trip(double km, double free_min, int hour) {
    return {{28.45, 77.58}, {28.55, 77.20}, "Bennett University", "Hauz Khas", km, free_min, hour, true};
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

// ---------------------------------------------------------------- adapters

TEST(FareAdapters, RateCardFormulaAppliesSurgeByHour) {
    json surge = json::array();
    for (int h = 0; h < 24; ++h) surge.push_back(h == 9 ? 1.25 : 1.0);
    const auto e = fares::FareEngine::from_json(one_product("uber", cab_card(), surge), transport());
    const auto* p = e.find("test", "x");
    ASSERT_NE(p, nullptr);

    // 50 + 12 * 10 + 1.5 * 30 = 215
    EXPECT_DOUBLE_EQ(p->fare(ride(10, 30, 20, 14)), 215);
    const auto q = p->quote(ride(10, 30, 20, 9));
    EXPECT_DOUBLE_EQ(q.surge, 1.25);
    EXPECT_EQ(q.fare_rs, 269);       // 215 * 1.25 = 268.75
    EXPECT_EQ(q.fare_low_rs, 228);   // -15%
    EXPECT_EQ(q.fare_high_rs, 309);  // +15%
    EXPECT_EQ(q.source, "estimate");
    EXPECT_EQ(q.basis, "approximate");
    EXPECT_DOUBLE_EQ(q.total_min(), 5 + 30);  // default pickup 5 min
}

TEST(FareAdapters, MinimumFareApplies) {
    const auto e = fares::FareEngine::from_json(one_product("ola", cab_card()), transport());
    EXPECT_DOUBLE_EQ(e.find("test", "x")->fare(ride(1, 3, 3, 12)), 85);  // 66.5 metered < 85 minimum
}

TEST(FareAdapters, DelhiMeterFollowsTheGovtRateCard) {
    const auto* meter = engine().find("delhi_auto", "meter");
    ASSERT_NE(meter, nullptr);
    EXPECT_EQ(meter->card().basis, "govt_rate_card");
    EXPECT_DOUBLE_EQ(meter->fare(ride(1.2, 5, 5, 12)), 30);    // inside the first 1.5 km
    EXPECT_DOUBLE_EQ(meter->fare(ride(5, 20, 20, 12)), 68.5);  // 30 + 11 * 3.5
    EXPECT_DOUBLE_EQ(meter->fare(ride(5, 30, 20, 12)), 76);    // + 10 min stuck in traffic * 0.75
    EXPECT_DOUBLE_EQ(meter->fare(ride(5, 20, 20, 23)), 68.5 * 1.25);  // night 23:00-05:00
    EXPECT_DOUBLE_EQ(meter->fare(ride(5, 20, 20, 2)), 68.5 * 1.25);
    EXPECT_DOUBLE_EQ(meter->fare(ride(5, 20, 20, 5)), 68.5);
    for (int h = 0; h < 24; ++h) EXPECT_DOUBLE_EQ(meter->card().surge[static_cast<std::size_t>(h)], 1.0);
    EXPECT_FALSE(meter->quote(ride(5, 20, 20, 12)).link.has_value());  // hailed on the street
}

TEST(FareAdapters, EveryProductGivesAnHonestEstimate) {
    const auto quotes = engine().quotes(trip(10, 20, 10));
    std::set<std::string> names;
    for (const auto& q : quotes) {
        names.insert(q.provider + "/" + q.product);
        EXPECT_EQ(q.source, "estimate");
        EXPECT_TRUE(q.basis == "approximate" || q.basis == "fitted" || q.basis == "govt_rate_card") << q.basis;
        EXPECT_LE(q.fare_low_rs, q.fare_rs);
        EXPECT_GE(q.fare_high_rs, q.fare_rs);
        EXPECT_GT(q.fare_rs, 0);
        EXPECT_GT(q.pickup_eta_min, 0);
        EXPECT_GT(q.ride_min, 0);
        EXPECT_FALSE(q.provider_name.empty());
        EXPECT_FALSE(q.product_name.empty());
    }
    const std::set<std::string> expected = {"uber/go",     "uber/premier", "uber/xl",     "ola/mini",         "ola/prime",
                                            "rapido/bike", "rapido/auto",  "rapido/cab",  "delhi_auto/meter"};
    EXPECT_EQ(names, expected);
    EXPECT_TRUE(std::is_sorted(quotes.begin(), quotes.end(),
                               [](const auto& a, const auto& b) { return a.fare_rs < b.fare_rs; }));
}

TEST(FareAdapters, UberUniversalLinkCarriesPickupAndDrop) {
    const auto q = engine().find("uber", "go")->quote(ride(10, 30, 20, 10));
    ASSERT_TRUE(q.link.has_value());
    EXPECT_EQ(q.link->url,
              "https://m.uber.com/ul/?action=setPickup&pickup[latitude]=28.450000&pickup[longitude]=77.580000"
              "&pickup[nickname]=Bennett%20University&dropoff[latitude]=28.550000&dropoff[longitude]=77.200000"
              "&dropoff[nickname]=Hauz%20Khas");
    EXPECT_EQ(q.link->app_url.rfind("uber://?action=setPickup&", 0), 0u);
    EXPECT_TRUE(q.link->prefilled);
    EXPECT_TRUE(q.link->documented);
}

TEST(FareAdapters, OlaAndRapidoLinks) {
    const auto mini = engine().find("ola", "mini")->quote(ride(10, 30, 20, 10));
    ASSERT_TRUE(mini.link.has_value());
    EXPECT_EQ(mini.link->url.rfind("https://book.olacabs.com/?", 0), 0u);
    for (const char* part : {"category=mini", "lat=28.450000", "lng=77.580000", "drop_lat=28.550000", "drop_lng=77.200000"}) {
        EXPECT_NE(mini.link->url.find(part), std::string::npos) << part;
    }
    EXPECT_EQ(mini.link->app_url.rfind("olacabs://app/launch?", 0), 0u);
    EXPECT_TRUE(mini.link->prefilled);
    EXPECT_FALSE(mini.link->documented);  // Ola no longer documents this format

    const auto prime = engine().find("ola", "prime")->quote(ride(10, 30, 20, 10));
    EXPECT_NE(prime.link->url.find("category=prime"), std::string::npos);

    const auto bike = engine().find("rapido", "bike")->quote(ride(10, 30, 20, 10));
    ASSERT_TRUE(bike.link.has_value());
    EXPECT_FALSE(bike.link->prefilled);
    EXPECT_EQ(bike.link->url.find("28.45"), std::string::npos);  // no pre-fill format exists
}

TEST(FareAdapters, UrlEncodeKeepsLinksSafe) {
    EXPECT_EQ(fares::url_encode("Hauz Khas & Co/1"), "Hauz%20Khas%20%26%20Co%2F1");
    EXPECT_EQ(fares::url_encode("a-b_c.d~e"), "a-b_c.d~e");
    EXPECT_EQ(fares::url_encode("\xE2\x82\xB9"), "%E2%82%B9");  // ₹
}

TEST(FareAdapters, QuoteJsonHasTheDocumentedFields) {
    const auto j = fares::to_json(engine().find("uber", "go")->quote(ride(10, 30, 20, 10)));
    for (const char* key : {"provider", "provider_name", "product", "product_name", "mode", "fare_rs", "fare_range_rs",
                            "surge", "pickup_eta_min", "ride_min", "total_min", "distance_km", "source", "basis",
                            "deep_link"}) {
        EXPECT_TRUE(j.contains(key)) << key;
    }
    EXPECT_EQ(j["fare_range_rs"].size(), 2u);
    EXPECT_EQ(j["deep_link"]["documented"], true);
    EXPECT_TRUE(fares::to_json(engine().find("delhi_auto", "meter")->quote(ride(5, 20, 20, 10)))["deep_link"].is_null());
}

// ---------------------------------------------------------------- engine

TEST(FareEngine, SearchFareIsTheCheapestProductOfTheMode) {
    for (auto m : graph::kAllRoadModes) {
        const auto r = engine().ride(m, trip(12, 25, 9));
        double cheapest = 1e9;
        for (const auto& p : engine().providers()) {
            if (p->mode() == m) cheapest = std::min(cheapest, p->fare(r));
        }
        EXPECT_DOUBLE_EQ(engine().search_fare(m, r), cheapest) << graph::to_string(m);
        EXPECT_NEAR(engine().quotes(m, r).front().fare_rs, cheapest, 0.5);
    }
}

TEST(FareEngine, RideTimeUsesTrafficAndTheModesSpeedFactor) {
    const auto& t = *transport();
    const auto r = engine().ride(graph::RoadMode::Auto, trip(12, 20, 9));
    EXPECT_DOUBLE_EQ(r.ride_min, 20 * t.traffic[9] * t.road_params(graph::RoadMode::Auto).speed_factor);
    EXPECT_DOUBLE_EQ(r.free_flow_min, r.ride_min / t.traffic[9]);
}

TEST(FareEngine, ModeWithoutProductsFallsBackToTheGenericCard) {
    const auto e = fares::FareEngine::from_json(one_product("uber", cab_card()), transport());
    const auto r = e.ride(graph::RoadMode::Bike, trip(6, 15, 12));
    const auto q = e.quotes(graph::RoadMode::Bike, r);
    ASSERT_EQ(q.size(), 1u);
    EXPECT_EQ(q[0].provider, "generic");
    EXPECT_EQ(q[0].basis, "generic_rate_card");
    EXPECT_EQ(q[0].source, "estimate");
    const double generic = transport()->road_fare(graph::RoadMode::Bike, 6, r.ride_min);
    EXPECT_DOUBLE_EQ(e.search_fare(graph::RoadMode::Bike, r), generic);
    EXPECT_EQ(q[0].fare_rs, std::round(generic));
}

TEST(FareEngine, LongTripsOnlyGetModesThatGoThatFar) {
    for (const auto& q : engine().quotes(trip(50, 70, 12))) EXPECT_EQ(q.mode, graph::RoadMode::Cab);  // auto/bike max 35/40 km
    EXPECT_EQ(engine().quotes(trip(50, 70, 12)).size(), 6u);
}

TEST(FareEngine, RejectsBadConfig) {
    auto expect_error = [](const json& j, const std::string& part) {
        try {
            fares::FareEngine::from_json(j, transport());
            ADD_FAILURE() << "accepted: " << j.dump();
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find(part), std::string::npos) << e.what();
        }
    };
    expect_error(json::object(), "providers");
    expect_error(one_product("lyft", cab_card()), "unknown fare adapter");
    expect_error(one_product("uber", cab_card(), json::array({1, 1, 1})), "24 numbers");
    auto negative = cab_card();
    negative["per_km"] = -3;
    expect_error(one_product("uber", negative), "providers.test: products.x: per_km");
    auto heli = cab_card();
    heli["mode"] = "helicopter";
    expect_error(one_product("uber", heli), "mode must be");
    auto guess = cab_card();
    guess["basis"] = "guess";
    expect_error(one_product("uber", guess), "basis");
    json surge = json::array();
    for (int h = 0; h < 24; ++h) surge.push_back(h == 18 ? 1.5 : 1.0);
    expect_error(one_product("meter", cab_card(), surge), "no surge");
}

TEST(FareEngine, ShippedConfigLoadsAndIsLabelledApproximate) {
    const json j = json::parse(read_file(SAFAR_TEST_CONFIG_DIR "/providers.json"));
    EXPECT_NE(j["_note"].get<std::string>().find("APPROXIMATE"), std::string::npos);
    EXPECT_EQ(engine().providers().size(), 9u);
}

// ---------------------------------------------------------------- planner

TEST(PlannerFares, RoadLegsListProviderQuotesAndUseTheCheapestPerMode) {
    Config cfg;
    cfg.data_dir = SAFAR_TEST_DATA_DIR;
    cfg.transport_file = SAFAR_TEST_CONFIG_DIR "/transport.json";
    const auto ctx = AppContext::load(cfg);
    const auto& ds = *ctx.dataset;
    journey::JourneyRequest req;
    req.from = {journey::Endpoint::Type::Place, static_cast<std::size_t>(ds.place("india-gate") - ds.places().data())};
    req.to = {journey::Endpoint::Type::Place, static_cast<std::size_t>(ds.place("hauz-khas-village") - ds.places().data())};
    req.hour = 10;
    const auto result = ctx.planner->plan(req);

    bool checked = false;
    for (const auto& route : result.routes) {
        for (std::size_t i = 0; i < route.itinerary.legs.size(); ++i) {
            const auto& leg = route.itinerary.legs[i];
            if (leg.kind != search::Leg::Kind::Road) continue;
            const auto& opts = route.road_options[i];
            ASSERT_FALSE(opts.empty());
            double cheapest_same_mode = 1e9;
            bool has_link = false;
            for (const auto& o : opts) {
                EXPECT_NE(o.provider, "generic");
                EXPECT_EQ(o.source, "estimate");
                has_link |= o.link.has_value() && o.link->prefilled;
                if (o.mode == leg.road_mode) cheapest_same_mode = std::min(cheapest_same_mode, o.fare_rs);
            }
            EXPECT_TRUE(has_link);
            EXPECT_NEAR(leg.fare_rs, cheapest_same_mode, 1.0);  // the search priced the leg with that product
            checked = true;
        }
    }
    EXPECT_TRUE(checked);
}

// ---------------------------------------------------------------- scripts/fit_fares.py

class FitFaresScript : public ::testing::Test {
protected:
    void SetUp() override {
        if (std::string(SAFAR_PYTHON).empty()) GTEST_SKIP() << "python3 not found by CMake";
        dir_ = fs::temp_directory_path() /
               ("safar_fit_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        providers_ = dir_ / "providers.json";
        fs::copy_file(SAFAR_TEST_CONFIG_DIR "/providers.json", providers_);
    }
    void TearDown() override {
        if (!dir_.empty()) fs::remove_all(dir_);
    }

    // Runs the script; returns its exit code, stdout in out_.
    int run(const fs::path& samples, const std::string& flags) {
        const auto out = dir_ / "out.txt";
        const std::string cmd = std::string("\"") + SAFAR_PYTHON + "\" -I \"" SAFAR_SCRIPTS_DIR "/fit_fares.py\" --samples \"" +
                                samples.string() + "\" --providers \"" + providers_.string() +
                                "\" --today 2026-10-27 " + flags + " > \"" + out.string() + "\" 2> \"" +
                                (dir_ / "err.txt").string() + "\"";
        const int rc = std::system(cmd.c_str());
        out_ = read_file(out);
        err_ = read_file(dir_ / "err.txt");
        return rc;
    }

    // Quotes generated from a known model, so the fit has an exact answer.
    fs::path synthetic_samples() {
        const auto path = dir_ / "samples.csv";
        std::ofstream f(path);
        f << "provider,product,from,to,km,minutes,hour,quoted_fare,date,source\n";
        const double km[] = {3, 6, 9, 12, 15, 20, 25, 30};
        const double min[] = {12, 15, 30, 28, 45, 40, 70, 55};
        const int hour[] = {14, 9, 14, 18, 12, 9, 15, 18};
        const auto& surge = engine().find("uber", "go")->card().surge;
        for (int i = 0; i < 8; ++i) {
            const double fare = (40 + 13 * km[i] + 1.2 * min[i]) * surge[static_cast<std::size_t>(hour[i])];
            f << "uber,go,A,B," << km[i] << "," << min[i] << "," << hour[i] << "," << fare << ",2026-10-2" << i % 7
              << ",app\n";
        }
        f << "uber,go,A,B,5,10,12,9999,2026-10-20,example\n";  // must be ignored
        f << "rapido,bike,A,B,4,10,9,52,2026-10-20,app\n";
        f << "rapido,bike,A,B,8,20,9,90,2026-10-20,app\n";
        f << "delhi_auto,meter,A,B,5,20,12,70,2026-10-20,app\n";
        return path;
    }

    fs::path dir_, providers_;
    std::string out_, err_;
};

TEST_F(FitFaresScript, RecoversKnownCoefficientsAndWritesProvidersJson) {
    ASSERT_EQ(run(synthetic_samples(), "--json"), 0) << err_;
    const json report = json::parse(out_);
    EXPECT_EQ(report["used_rows"], 11);
    EXPECT_EQ(report["skipped_example_rows"], 1);
    ASSERT_EQ(report["products"].size(), 1u);
    const auto& go = report["products"][0];
    EXPECT_EQ(go["provider"], "uber");
    EXPECT_EQ(go["product"], "go");
    EXPECT_EQ(go["n"], 8);
    EXPECT_NEAR(go["base"].get<double>(), 40, 0.05);
    EXPECT_NEAR(go["per_km"].get<double>(), 13, 0.01);
    EXPECT_NEAR(go["per_min"].get<double>(), 1.2, 0.01);
    EXPECT_LT(go["mae_after_rs"].get<double>(), 0.5);
    EXPECT_GT(go["mae_before_rs"].get<double>(), go["mae_after_rs"].get<double>());
    EXPECT_FALSE(go["few_samples"].get<bool>());

    std::map<std::string, std::string> skipped;
    for (const auto& s : report["skipped"]) skipped[s["provider"].get<std::string>() + "/" + s["product"].get<std::string>()] = s["reason"];
    EXPECT_NE(skipped["rapido/bike"].find("at least 3"), std::string::npos);
    EXPECT_NE(skipped["delhi_auto/meter"].find("government"), std::string::npos);
    EXPECT_EQ(report["wrote"], providers_.string());

    // The written file keeps everything else and still loads in the server.
    const json written = json::parse(read_file(providers_));
    const auto& card = written["providers"]["uber"]["products"]["go"];
    EXPECT_EQ(card["basis"], "fitted");
    EXPECT_EQ(card["base_km"], 0);
    EXPECT_EQ(card["fit"]["samples"], 8);
    EXPECT_EQ(card["fit"]["fitted_on"], "2026-10-27");
    EXPECT_FALSE(card["fit"]["includes_examples"].get<bool>());
    EXPECT_EQ(written["providers"]["ola"]["products"]["mini"]["per_km"], 12.5);
    const auto e = fares::FareEngine::load(providers_, transport());
    EXPECT_EQ(e.find("uber", "go")->card().basis, "fitted");
    EXPECT_NEAR(e.find("uber", "go")->card().per_km, 13, 0.01);
}

TEST_F(FitFaresScript, TableShowsMaePerProductAndDryRunWritesNothing) {
    const auto before = read_file(providers_);
    ASSERT_EQ(run(synthetic_samples(), "--dry-run"), 0) << err_;
    for (const char* part : {"Fare model fit:", "MAE before", "MAE after", "MAPE", "uber/go", "skipped: rapido/bike",
                             "dry run"}) {
        EXPECT_NE(out_.find(part), std::string::npos) << part << "\n" << out_;
    }
    EXPECT_EQ(read_file(providers_), before);
}

TEST_F(FitFaresScript, RejectsBadRowsWithTheLineNumber) {
    const auto path = dir_ / "bad.csv";
    std::ofstream(path) << "provider,product,from,to,km,minutes,hour,quoted_fare,date,source\n"
                        << "uber,go,A,B,5,10,25,120,2026-10-20,app\n";
    EXPECT_NE(run(path, "--json"), 0);
    EXPECT_NE(err_.find("line 2"), std::string::npos) << err_;
}

TEST_F(FitFaresScript, ShippedSamplesFileHasTheExpectedFormat) {
    const fs::path samples = SAFAR_TEST_DATA_DIR "/../fare_samples.csv";
    std::ifstream in(samples);
    std::string header;
    std::getline(in, header);
    EXPECT_EQ(header, "provider,product,from,to,km,minutes,hour,quoted_fare,date,source");
    for (std::string line; std::getline(in, line);) {
        if (line.empty()) continue;
        const auto source = line.substr(line.rfind(',') + 1);
        EXPECT_TRUE(source == "example" || source == "app") << line;
    }
    const auto before = read_file(providers_);
    ASSERT_EQ(run(samples, "--json --dry-run"), 0) << err_;
    const json report = json::parse(out_);
    EXPECT_TRUE(report.contains("products"));
    EXPECT_TRUE(report["wrote"].is_null());
    EXPECT_EQ(read_file(providers_), before);
}
