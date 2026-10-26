#pragma once

// JSON envelope helpers shared by every route file.
//   { "ok": true,  "data": ..., "meta": { "took_ms": .., "request_id": .. } }
//   { "ok": false, "error": { "code": .., "message": .. }, "meta": {..} }

#include <chrono>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace safar::api {

using Clock = std::chrono::steady_clock;

inline nlohmann::json ok_body(const nlohmann::json& data) {
    return {{"ok", true}, {"data", data}, {"meta", nlohmann::json::object()}};
}

inline nlohmann::json error_body(const std::string& code, const std::string& message) {
    return {{"ok", false}, {"error", {{"code", code}, {"message", message}}}, {"meta", nlohmann::json::object()}};
}

inline void send_json(httplib::Response& res, int status, const nlohmann::json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

inline void send_error(httplib::Response& res, int status, const std::string& code, const std::string& message) {
    send_json(res, status, error_body(code, message));
}

// Sends a success envelope with timing + request id filled into meta.
inline void send_ok(httplib::Response& res, Clock::time_point t0, const nlohmann::json& data) {
    nlohmann::json body = ok_body(data);
    body["meta"]["took_ms"] = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    body["meta"]["request_id"] = res.get_header_value("X-Request-Id");
    send_json(res, 200, body);
}

}  // namespace safar::api
