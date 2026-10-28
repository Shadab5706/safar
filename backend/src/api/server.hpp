#pragma once

#include <chrono>
#include <memory>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/response.hpp"
#include "app/context.hpp"
#include "disruption/closures.hpp"
#include "util/config.hpp"

namespace safar::api {

// Owns the HTTP server and registers every /api/v1 route.
// Responses always use one envelope:
//   { "ok": true,  "data": ..., "meta": { "took_ms": .., "request_id": .. } }
//   { "ok": false, "error": { "code": .., "message": .. }, "meta": {..} }
class Server {
public:
    Server(Config config, AppContext ctx);

    // Blocks until stop() is called.
    bool listen();
    void stop();

    httplib::Server& raw() { return http_; }

private:
    void register_routes();
    void register_lab_routes();      // api/lab_routes.cpp
    void register_journey_routes();  // api/journey_routes.cpp
    void register_fare_routes();     // api/fare_routes.cpp
    void register_disruption_routes();  // api/disruption_routes.cpp
    void register_trip_routes();        // api/trip_routes.cpp
    void register_middleware();

    Config config_;
    AppContext ctx_;
    // Line closures are server state, not loaded data: every Server starts with none.
    disruption::Store disruptions_;
    httplib::Server http_;
    std::chrono::steady_clock::time_point started_at_;
};

}  // namespace safar::api
