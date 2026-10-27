#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "data/dataset.hpp"
#include "journey/planner.hpp"

namespace safar::api {

// Thrown by request parsing; the route turns it into an error envelope.
struct BadRequest {
    int status;
    std::string code;
    std::string message;
};

// "bennett-university", "place:<id>" or "station:<id>".
journey::Endpoint parse_endpoint_id(const data::Dataset& ds, const std::string& id);
// body[key] as an id string or {"type": "place"|"station", "id": ...}.
journey::Endpoint parse_endpoint(const nlohmann::json& body, const char* key, const data::Dataset& ds);

struct DepartTime {
    std::string at;  // "YYYY-MM-DDTHH:MM", local time
    int hour = 0;
    bool sunday = false;
};

// iso = "YYYY-MM-DDTHH:MM" (seconds and zone suffix ignored), nullptr = now.
// field names the parameter in error messages.
DepartTime parse_depart(const std::string* iso, const char* field);

// {"type", "id", "name", "lat", "lng"} for a place or station.
nlohmann::json endpoint_ref(const data::Dataset& ds, const journey::Endpoint& e);

}  // namespace safar::api
