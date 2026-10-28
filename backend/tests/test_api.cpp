#include <gtest/gtest.h>

#include <set>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"

namespace {

// Starts the real server on a free port for the duration of one test.
class ApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        safar::Config cfg;
        cfg.host = "127.0.0.1";
        cfg.data_dir = SAFAR_TEST_DATA_DIR;
        cfg.transport_file = SAFAR_TEST_CONFIG_DIR "/transport.json";
        static const safar::AppContext ctx = safar::AppContext::load(cfg);
        server_ = std::make_unique<safar::api::Server>(cfg, ctx);
        port_ = server_->raw().bind_to_any_port(cfg.host);
        thread_ = std::thread([this] { server_->raw().listen_after_bind(); });
        server_->raw().wait_until_ready();
    }
    void TearDown() override {
        server_->stop();
        thread_.join();
    }

    httplib::Client client() { return httplib::Client("127.0.0.1", port_); }

    std::unique_ptr<safar::api::Server> server_;
    std::thread thread_;
    int port_ = 0;
};

}  // namespace

TEST_F(ApiTest, HealthReturnsOkEnvelope) {
    auto res = client().Get("/api/v1/health");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body["ok"].get<bool>());
    EXPECT_EQ(body["data"]["status"], "ok");
    EXPECT_FALSE(res->get_header_value("X-Request-Id").empty());
}

TEST_F(ApiTest, UnknownRouteReturnsJson404) {
    auto res = client().Get("/api/v1/nope");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    auto body = nlohmann::json::parse(res->body);
    EXPECT_FALSE(body["ok"].get<bool>());
    EXPECT_EQ(body["error"]["code"], "not_found");
}

TEST_F(ApiTest, HealthReportsLoadedData) {
    auto res = client().Get("/api/v1/health");
    ASSERT_TRUE(res);
    auto data = nlohmann::json::parse(res->body)["data"]["data"];
    EXPECT_GT(data["stations"].get<int>(), 250);
    EXPECT_GT(data["places"].get<int>(), 100);
}

TEST_F(ApiTest, PlacesAutocomplete) {
    auto res = client().Get("/api/v1/places?q=rajiv&limit=5");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    auto data = nlohmann::json::parse(res->body)["data"];
    ASSERT_FALSE(data.empty());
    EXPECT_LE(data.size(), 5u);
    bool station_found = false;
    for (const auto& r : data) station_found |= (r["kind"] == "station" && r["id"] == "rajiv-chowk");
    EXPECT_TRUE(station_found);
}

TEST_F(ApiTest, PlacesRejectsTooShortQuery) {
    auto res = client().Get("/api/v1/places?q=a");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_EQ(nlohmann::json::parse(res->body)["error"]["code"], "invalid_query");
}

TEST_F(ApiTest, MetroLinesHaveColoursAndStations) {
    auto res = client().Get("/api/v1/metro/lines");
    ASSERT_TRUE(res);
    auto lines = nlohmann::json::parse(res->body)["data"];
    EXPECT_GE(lines.size(), 12u);
    for (const auto& l : lines) {
        EXPECT_EQ(l["color"].get<std::string>().front(), '#');
        EXPECT_GE(l["stations"].size(), 2u);
    }
}

TEST_F(ApiTest, StationById) {
    auto ok = client().Get("/api/v1/metro/stations/kashmere-gate");
    ASSERT_TRUE(ok);
    EXPECT_EQ(ok->status, 200);
    EXPECT_EQ(nlohmann::json::parse(ok->body)["data"]["name"], "Kashmere Gate");

    auto missing = client().Get("/api/v1/metro/stations/atlantis");
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status, 404);
    EXPECT_EQ(nlohmann::json::parse(missing->body)["error"]["code"], "station_not_found");
}

namespace {
nlohmann::json post_json(httplib::Client& c, const std::string& path, const nlohmann::json& body, int& status) {
    auto res = c.Post(path, body.dump(), "application/json");
    status = res ? res->status : -1;
    return res ? nlohmann::json::parse(res->body) : nlohmann::json();
}
}  // namespace

TEST_F(ApiTest, LabCompareRunsAllFiveAlgorithms) {
    auto c = client();
    int status = 0;
    auto body = post_json(c, "/api/v1/lab/compare", {{"from", "rajiv-chowk"}, {"to", "hauz-khas"}}, status);
    ASSERT_EQ(status, 200);
    const auto& data = body["data"];
    ASSERT_EQ(data["results"].size(), 5u);
    double ucs = -1, astar = -2;
    for (const auto& r : data["results"]) {
        EXPECT_TRUE(r["found"].get<bool>());
        EXPECT_FALSE(r["expansion"].empty());
        EXPECT_GE(r["optimality_gap_pct"].get<double>(), 0.0);
        if (r["algorithm"] == "ucs") ucs = r["cost"];
        if (r["algorithm"] == "astar") astar = r["cost"];
    }
    EXPECT_DOUBLE_EQ(ucs, astar);
    EXPECT_DOUBLE_EQ(data["optimal_cost"].get<double>(), ucs);
}

TEST_F(ApiTest, LabCompareSubsetWithoutExpansion) {
    auto c = client();
    int status = 0;
    auto body = post_json(c, "/api/v1/lab/compare",
                          {{"from", "kashmere-gate"}, {"to", "botanical-garden"}, {"algorithms", {"astar"}},
                           {"include_expansion", false}},
                          status);
    ASSERT_EQ(status, 200);
    ASSERT_EQ(body["data"]["results"].size(), 1u);
    EXPECT_FALSE(body["data"]["results"][0].contains("expansion"));
    EXPECT_FALSE(body["data"]["results"][0]["itinerary"]["legs"].empty());
}

TEST_F(ApiTest, LabCompareValidatesInput) {
    auto c = client();
    int status = 0;
    auto unknown_algo = post_json(c, "/api/v1/lab/compare",
                                  {{"from", "rajiv-chowk"}, {"to", "hauz-khas"}, {"algorithms", {"dijkstra"}}}, status);
    EXPECT_EQ(status, 400);
    EXPECT_EQ(unknown_algo["error"]["code"], "unknown_algorithm");

    auto unknown_station = post_json(c, "/api/v1/lab/compare", {{"from", "atlantis"}, {"to", "hauz-khas"}}, status);
    EXPECT_EQ(status, 404);
    EXPECT_EQ(unknown_station["error"]["code"], "station_not_found");

    auto bad_speed = post_json(c, "/api/v1/lab/compare",
                               {{"from", "rajiv-chowk"}, {"to", "hauz-khas"}, {"heuristic_speed_kmh", -5}}, status);
    EXPECT_EQ(status, 400);
    EXPECT_EQ(bad_speed["error"]["code"], "invalid_field");

    auto res = c.Post("/api/v1/lab/compare", "{not json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_EQ(nlohmann::json::parse(res->body)["error"]["code"], "bad_json");
}

TEST_F(ApiTest, JourneysBennettToHauzKhasVillage) {
    auto c = client();
    int status = 0;
    auto body = post_json(c, "/api/v1/journeys",
                          {{"from", {{"type", "place"}, {"id", "bennett-university"}}},
                           {"to", "hauz-khas-village"},
                           {"depart_at", "2026-10-27T09:00"}},
                          status);
    ASSERT_EQ(status, 200) << body.dump();
    const auto& data = body["data"];
    EXPECT_EQ(data["from"]["name"], "Bennett University");
    EXPECT_EQ(data["depart"]["hour"], 9);
    EXPECT_FALSE(data["depart"]["sunday_fares"].get<bool>());  // 27 Oct 2026 is a Tuesday
    ASSERT_GE(data["routes"].size(), 3u);
    for (const auto& r : data["routes"]) {
        EXPECT_GT(r["total_min"].get<double>(), 0);
        EXPECT_GT(r["fare_rs"].get<double>(), 0);
        EXPECT_FALSE(r["summary"].get<std::string>().empty());
        for (const auto& leg : r["legs"]) EXPECT_FALSE(leg["points"].empty());
    }
    const auto& tags = data["routes"][0]["tags"];
    EXPECT_NE(std::find(tags.begin(), tags.end(), "recommended"), tags.end());
}

TEST_F(ApiTest, JourneysValidatesInput) {
    auto c = client();
    int status = 0;
    auto bad_mode = post_json(c, "/api/v1/journeys",
                              {{"from", "india-gate"}, {"to", "red-fort"}, {"modes", {"helicopter"}}}, status);
    EXPECT_EQ(status, 400);
    EXPECT_EQ(bad_mode["error"]["code"], "unknown_mode");

    auto unknown = post_json(c, "/api/v1/journeys", {{"from", "place:atlantis"}, {"to", "red-fort"}}, status);
    EXPECT_EQ(status, 404);
    EXPECT_EQ(unknown["error"]["code"], "place_not_found");

    auto same = post_json(c, "/api/v1/journeys", {{"from", "india-gate"}, {"to", "india-gate"}}, status);
    EXPECT_EQ(status, 400);
    EXPECT_EQ(same["error"]["code"], "same_endpoints");

    post_json(c, "/api/v1/journeys", {{"from", "india-gate"}, {"to", "red-fort"}, {"depart_at", "2026-02-30T09:00"}},
              status);
    EXPECT_EQ(status, 400);

    post_json(c, "/api/v1/journeys", {{"from", "india-gate"}, {"to", "red-fort"}, {"preference", 3}}, status);
    EXPECT_EQ(status, 400);
}

TEST_F(ApiTest, FaresEstimateListsEveryProvider) {
    auto res = client().Get("/api/v1/fares/estimate?from=india-gate&to=place:hauz-khas-village&at=2026-10-27T09:00");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    const auto data = nlohmann::json::parse(res->body)["data"];
    EXPECT_EQ(data["from"]["name"], "India Gate");
    EXPECT_EQ(data["depart"]["hour"], 9);
    EXPECT_GT(data["trip"]["distance_km"].get<double>(), 3);
    EXPECT_FALSE(data["note"].get<std::string>().empty());
    const auto& quotes = data["quotes"];
    ASSERT_EQ(quotes.size(), 9u);  // 3 Uber + 2 Ola + 3 Rapido + auto meter
    double last = 0;
    bool uber_link = false;
    for (const auto& q : quotes) {
        EXPECT_EQ(q["source"], "estimate");
        EXPECT_EQ(q["fare_range_rs"].size(), 2u);
        EXPECT_GE(q["fare_rs"].get<double>(), last);
        last = q["fare_rs"].get<double>();
        if (q["provider"] == "uber") {
            uber_link = q["deep_link"]["url"].get<std::string>().rfind("https://m.uber.com/ul/?action=setPickup", 0) == 0;
        }
        if (q["provider"] == "delhi_auto") EXPECT_TRUE(q["deep_link"].is_null());
    }
    EXPECT_TRUE(uber_link);
}

TEST_F(ApiTest, FaresEstimateValidatesInput) {
    auto c = client();
    auto missing = c.Get("/api/v1/fares/estimate?from=india-gate");
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status, 400);
    auto unknown = c.Get("/api/v1/fares/estimate?from=place:atlantis&to=red-fort");
    EXPECT_EQ(unknown->status, 404);
    EXPECT_EQ(nlohmann::json::parse(unknown->body)["error"]["code"], "place_not_found");
    auto same = c.Get("/api/v1/fares/estimate?from=red-fort&to=red-fort");
    EXPECT_EQ(nlohmann::json::parse(same->body)["error"]["code"], "same_endpoints");
    auto bad_time = c.Get("/api/v1/fares/estimate?from=india-gate&to=red-fort&at=2026-13-01T09:00");
    EXPECT_EQ(bad_time->status, 400);
}

TEST_F(ApiTest, JourneyRoadLegsHaveProviderQuotesWithDeepLinks) {
    auto c = client();
    int status = 0;
    auto body = post_json(c, "/api/v1/journeys",
                          {{"from", "india-gate"}, {"to", "hauz-khas-village"}, {"depart_at", "2026-10-27T10:00"}}, status);
    ASSERT_EQ(status, 200) << body.dump();
    EXPECT_FALSE(body["data"]["fare_note"].get<std::string>().empty());
    bool checked = false;
    for (const auto& r : body["data"]["routes"]) {
        for (const auto& leg : r["legs"]) {
            if (leg["kind"] != "road") continue;
            ASSERT_TRUE(leg.contains("options"));
            std::set<std::string> providers;
            for (const auto& o : leg["options"]) {
                providers.insert(o["provider"].get<std::string>());
                EXPECT_EQ(o["source"], "estimate");
                EXPECT_TRUE(o.contains("product_name"));
                EXPECT_TRUE(o.contains("pickup_eta_min"));
                EXPECT_TRUE(o.contains("deep_link"));
            }
            EXPECT_TRUE(providers.count("uber") || providers.count("rapido")) << leg.dump();
            checked = true;
        }
    }
    EXPECT_TRUE(checked);
}

TEST_F(ApiTest, DisruptionsLifecycle) {
    auto c = client();
    int status = 0;
    auto list = nlohmann::json::parse(c.Get("/api/v1/disruptions")->body);
    EXPECT_TRUE(list["data"]["items"].empty()) << "every server starts with no closures";

    auto added = post_json(c, "/api/v1/disruptions", {{"type", "station"}, {"station", "rajiv-chowk"}}, status);
    EXPECT_EQ(status, 201);
    EXPECT_TRUE(added["data"]["added"].get<bool>());
    EXPECT_EQ(added["data"]["closure"]["id"], "station:rajiv-chowk");
    EXPECT_EQ(added["data"]["closure"]["station"]["name"], "Rajiv Chowk");

    auto again = post_json(c, "/api/v1/disruptions", {{"type", "station"}, {"station", "rajiv-chowk"}}, status);
    EXPECT_EQ(status, 200);
    EXPECT_FALSE(again["data"]["added"].get<bool>());

    auto seg = post_json(c, "/api/v1/disruptions",
                         {{"type", "segment"}, {"line", "yellow"}, {"from", "hauz-khas"}, {"to", "green-park"}}, status);
    EXPECT_EQ(status, 201);
    EXPECT_EQ(seg["data"]["closure"]["id"], "segment:yellow:green-park:hauz-khas");
    EXPECT_EQ(seg["data"]["closure"]["line"]["color"].get<std::string>().front(), '#');
    EXPECT_EQ(seg["data"]["items"].size(), 2u);

    post_json(c, "/api/v1/disruptions", {{"type", "segment"}, {"line", "yellow"}, {"from", "rajiv-chowk"}, {"to", "hauz-khas"}},
              status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/disruptions", {{"type", "station"}, {"station", "atlantis"}}, status);
    EXPECT_EQ(status, 404);
    post_json(c, "/api/v1/disruptions", {{"type", "line"}}, status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/disruptions", {{"type", "station"}}, status);
    EXPECT_EQ(status, 400);

    auto removed = c.Delete("/api/v1/disruptions/station:rajiv-chowk");
    ASSERT_TRUE(removed);
    EXPECT_EQ(removed->status, 200);
    EXPECT_EQ(nlohmann::json::parse(removed->body)["data"]["items"].size(), 1u);
    EXPECT_EQ(c.Delete("/api/v1/disruptions/station:rajiv-chowk")->status, 404);

    auto cleared = c.Delete("/api/v1/disruptions");
    EXPECT_EQ(cleared->status, 200);
    EXPECT_TRUE(nlohmann::json::parse(cleared->body)["data"]["items"].empty());
}

TEST_F(ApiTest, LabAndJourneysRespectClosures) {
    auto c = client();
    int status = 0;
    const nlohmann::json lab_req = {{"from", "rajiv-chowk"}, {"to", "hauz-khas"}, {"algorithms", {"ucs", "astar"}}};
    auto open = post_json(c, "/api/v1/lab/compare", lab_req, status);
    ASSERT_EQ(status, 200);
    EXPECT_EQ(open["data"]["closures"], 0);

    post_json(c, "/api/v1/disruptions", {{"type", "segment"}, {"line", "yellow"}, {"from", "green-park"}, {"to", "hauz-khas"}},
              status);
    ASSERT_EQ(status, 201);
    auto cut = post_json(c, "/api/v1/lab/compare", lab_req, status);
    ASSERT_EQ(status, 200);
    EXPECT_EQ(cut["data"]["closures"], 1);
    EXPECT_GT(cut["data"]["optimal_cost"].get<double>(), open["data"]["optimal_cost"].get<double>());
    for (const auto& r : cut["data"]["results"]) EXPECT_EQ(r["optimality_gap_pct"], 0.0);

    auto j = post_json(c, "/api/v1/journeys", {{"from", "station:rajiv-chowk"}, {"to", "station:hauz-khas"}}, status);
    ASSERT_EQ(status, 200);
    EXPECT_EQ(j["data"]["closures"], 1);
    EXPECT_FALSE(j["data"]["routes"].empty());
}

TEST_F(ApiTest, TripsOptimizeFindsTheBestOrder) {
    auto c = client();
    int status = 0;
    auto body = post_json(c, "/api/v1/trips/optimize",
                          {{"stops", {"bennett-university", "india-gate", "qutub-minar", "station:hauz-khas", "red-fort"}},
                           {"depart_at", "2026-10-27T10:00"}},
                          status);
    ASSERT_EQ(status, 200) << body.dump();
    const auto& d = body["data"];
    EXPECT_EQ(d["stops"].size(), 5u);
    EXPECT_EQ(d["stops"][3]["type"], "station");
    ASSERT_EQ(d["results"].size(), 2u);
    EXPECT_EQ(d["results"][0]["method"], "hill_climb");
    EXPECT_EQ(d["results"][1]["method"], "simulated_annealing");
    ASSERT_TRUE(d["optimum"].is_object());
    for (const auto& r : d["results"]) {
        EXPECT_EQ(r["order"].size(), 5u);
        EXPECT_EQ(r["order"][0], 0);
        EXPECT_GE(r["gap_pct"].get<double>(), 0.0);
        EXPECT_GE(r["score"].get<double>(), d["optimum"]["score"].get<double>());
        EXPECT_FALSE(r["curve"].empty());
    }
    EXPECT_LE(d["best"]["score"].get<double>(), d["input"]["score"].get<double>());
    ASSERT_EQ(d["best"]["legs"].size(), 4u);
    for (const auto& leg : d["best"]["legs"]) {
        EXPECT_FALSE(leg["route"]["legs"].empty());
        EXPECT_TRUE(leg["route"]["summary"].is_string());
    }
    EXPECT_TRUE(d["matrix"]["score"][0][0].is_null());
    EXPECT_GT(d["matrix"]["score"][0][1].get<double>(), 0.0);

    auto round = post_json(c, "/api/v1/trips/optimize",
                           {{"stops", {"india-gate", "red-fort", "qutub-minar"}}, {"method", "sa"}, {"round_trip", true}},
                           status);
    ASSERT_EQ(status, 200);
    EXPECT_EQ(round["data"]["results"].size(), 1u);
    EXPECT_EQ(round["data"]["best"]["legs"].size(), 3u) << "a round trip comes back to the start";
}

TEST_F(ApiTest, TripsOptimizeValidatesInput) {
    auto c = client();
    int status = 0;
    post_json(c, "/api/v1/trips/optimize", {{"stops", {"india-gate", "red-fort"}}}, status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/trips/optimize", {{"stops", {"india-gate", "red-fort", "india-gate"}}}, status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/trips/optimize", {{"stops", {"india-gate", "red-fort", "atlantis"}}}, status);
    EXPECT_EQ(status, 404);
    post_json(c, "/api/v1/trips/optimize", {{"stops", {"india-gate", "red-fort", "qutub-minar"}}, {"method", "genetic"}},
              status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/trips/optimize", {{"stops", {"india-gate", "red-fort", "qutub-minar"}}, {"iterations", 5}}, status);
    EXPECT_EQ(status, 400);
    post_json(c, "/api/v1/trips/optimize", {{"stops", "india-gate"}}, status);
    EXPECT_EQ(status, 400);
}
