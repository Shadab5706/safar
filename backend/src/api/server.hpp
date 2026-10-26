#pragma once

#include <chrono>
#include <memory>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/response.hpp"
#include "data/dataset.hpp"
#include "graph/graph.hpp"
#include "util/config.hpp"

namespace safar::api {

// Owns the HTTP server and registers every /api/v1 route.
// Responses always use one envelope:
//   { "ok": true,  "data": ..., "meta": { "took_ms": .., "request_id": .. } }
//   { "ok": false, "error": { "code": .., "message": .. }, "meta": {..} }
class Server {
public:
    Server(Config config, std::shared_ptr<const data::Dataset> dataset, std::shared_ptr<const graph::Graph> transit);

    // Blocks until stop() is called.
    bool listen();
    void stop();

    httplib::Server& raw() { return http_; }

private:
    void register_routes();
    void register_lab_routes();  // api/lab_routes.cpp
    void register_middleware();

    Config config_;
    std::shared_ptr<const data::Dataset> data_;
    std::shared_ptr<const graph::Graph> transit_;
    httplib::Server http_;
    std::chrono::steady_clock::time_point started_at_;
};

}  // namespace safar::api
