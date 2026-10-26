#include <gtest/gtest.h>

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
