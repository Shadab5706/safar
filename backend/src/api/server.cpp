#include "api/server.hpp"

#include <atomic>
#include <random>
#include <sstream>

#include <spdlog/spdlog.h>

namespace safar::api {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

std::string make_request_id() {
    static std::atomic<unsigned long> counter{0};
    static const unsigned long seed = std::random_device{}();
    std::ostringstream os;
    os << std::hex << (seed & 0xffff) << '-' << ++counter;
    return os.str();
}

void send_json(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

// Sends a success envelope with timing + request id filled into meta.
void send_ok(httplib::Response& res, Clock::time_point t0, const json& data) {
    json body = ok_body(data);
    body["meta"]["took_ms"] = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    body["meta"]["request_id"] = res.get_header_value("X-Request-Id");
    send_json(res, 200, body);
}

}  // namespace

json ok_body(const json& data) {
    return {{"ok", true}, {"data", data}, {"meta", json::object()}};
}

json error_body(const std::string& code, const std::string& message) {
    return {{"ok", false}, {"error", {{"code", code}, {"message", message}}}, {"meta", json::object()}};
}

Server::Server(Config config, std::shared_ptr<const data::Dataset> dataset)
    : config_(std::move(config)), data_(std::move(dataset)), started_at_(Clock::now()) {
    register_middleware();
    register_routes();
}

void Server::register_middleware() {
    // Stamp every request with an id + start time; handlers read them back in the logger.
    http_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("X-Request-Id", make_request_id());
        res.set_header("Access-Control-Allow-Origin", config_.cors_origin);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    http_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::info("{} {} -> {} [{}]", req.method, req.path, res.status, res.get_header_value("X-Request-Id"));
    });

    http_.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        std::string msg = "unknown error";
        try {
            std::rethrow_exception(ep);
        } catch (const json::exception& e) {
            send_json(res, 400, error_body("bad_json", e.what()));
            return;
        } catch (const std::exception& e) {
            msg = e.what();
        } catch (...) {
        }
        spdlog::error("unhandled exception: {}", msg);
        send_json(res, 500, error_body("internal", "internal server error"));
    });

    http_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.body.empty()) {
            send_json(res, res.status, error_body(res.status == 404 ? "not_found" : "http_error",
                                                  httplib::status_message(res.status)));
        }
    });
}

void Server::register_routes() {
    http_.Get("/api/v1/health", [this](const httplib::Request&, httplib::Response& res) {
        const auto t0 = Clock::now();
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started_at_).count();
        send_ok(res, t0, {
            {"status", "ok"},
            {"service", "safar"},
            {"version", SAFAR_VERSION},
            {"uptime_s", uptime},
            {"data", {{"lines", data_->lines().size()},
                      {"stations", data_->stations().size()},
                      {"segments", data_->segments().size()},
                      {"places", data_->places().size()}}},
        });
    });

    // Autocomplete for the from/to boxes. ?q=<text>&limit=<1..25>
    http_.Get("/api/v1/places", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const std::string q = req.get_param_value("q");
        if (q.size() < 2 || q.size() > 64) {
            send_json(res, 400, error_body("invalid_query", "q must be 2 to 64 characters"));
            return;
        }
        std::size_t limit = 10;
        if (req.has_param("limit")) {
            try {
                limit = std::clamp<std::size_t>(std::stoul(req.get_param_value("limit")), 1, 25);
            } catch (const std::exception&) {
                send_json(res, 400, error_body("invalid_limit", "limit must be a number"));
                return;
            }
        }

        json results = json::array();
        for (const auto& hit : data_->search(q, limit)) {
            if (hit.kind == data::SearchHit::Kind::Place) {
                json j = data::to_json(*hit.place);
                j["kind"] = "place";
                results.push_back(std::move(j));
            } else {
                json j = data::to_json(*hit.station);
                j["kind"] = "station";
                results.push_back(std::move(j));
            }
        }
        send_ok(res, t0, results);
    });

    // Full metro map: lines (with colours and station order) + station coordinates + segments.
    http_.Get("/api/v1/metro/lines", [this](const httplib::Request&, httplib::Response& res) {
        const auto t0 = Clock::now();
        json lines = json::array();
        for (const auto& l : data_->lines()) lines.push_back(data::to_json(l));
        send_ok(res, t0, lines);
    });

    http_.Get("/api/v1/metro/stations", [this](const httplib::Request&, httplib::Response& res) {
        const auto t0 = Clock::now();
        json stations = json::array();
        for (const auto& s : data_->stations()) stations.push_back(data::to_json(s));
        send_ok(res, t0, stations);
    });

    http_.Get(R"(/api/v1/metro/stations/([a-z0-9\-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto t0 = Clock::now();
        const auto* s = data_->station(req.matches[1]);
        if (!s) {
            send_json(res, 404, error_body("station_not_found", "no station with id " + std::string(req.matches[1])));
            return;
        }
        send_ok(res, t0, data::to_json(*s));
    });
}

bool Server::listen() {
    spdlog::info("safar {} listening on http://{}:{}", SAFAR_VERSION, config_.host, config_.port);
    return http_.listen(config_.host, config_.port);
}

void Server::stop() { http_.stop(); }

}  // namespace safar::api
