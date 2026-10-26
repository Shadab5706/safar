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
        static const auto dataset =
            std::make_shared<const safar::data::Dataset>(safar::data::Dataset::load(SAFAR_TEST_DATA_DIR));
        safar::Config cfg;
        cfg.host = "127.0.0.1";
        server_ = std::make_unique<safar::api::Server>(cfg, dataset);
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
