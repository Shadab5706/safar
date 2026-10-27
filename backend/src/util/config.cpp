#include "util/config.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace safar {

namespace {

const char* env(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : nullptr;
}

}  // namespace

Config Config::load(const std::filesystem::path& file) {
    Config c;

    if (std::filesystem::exists(file)) {
        std::ifstream in(file);
        auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/true, /*ignore_comments=*/true);
        c.host = j.value("host", c.host);
        c.port = j.value("port", c.port);
        c.data_dir = j.value("data_dir", c.data_dir.string());
        c.cors_origin = j.value("cors_origin", c.cors_origin);
        c.log_level = j.value("log_level", c.log_level);
        c.transport_file = file.parent_path() / j.value("transport_file", std::string("transport.json"));
        c.providers_file = file.parent_path() / j.value("providers_file", std::string("providers.json"));

        // Relative data paths are resolved against the config file's folder,
        // so the server works no matter which directory it is started from.
        if (c.data_dir.is_relative()) {
            c.data_dir = (file.parent_path() / c.data_dir).lexically_normal();
        }
    }

    if (auto v = env("SAFAR_HOST")) c.host = v;
    if (auto v = env("SAFAR_PORT")) c.port = std::stoi(v);
    if (auto v = env("SAFAR_DATA_DIR")) c.data_dir = v;
    if (auto v = env("SAFAR_CORS_ORIGIN")) c.cors_origin = v;
    if (auto v = env("SAFAR_LOG_LEVEL")) c.log_level = v;
    if (auto v = env("SAFAR_TRANSPORT_FILE")) c.transport_file = v;
    if (auto v = env("SAFAR_PROVIDERS_FILE")) c.providers_file = v;

    if (c.port <= 0 || c.port > 65535) {
        throw std::invalid_argument("port out of range: " + std::to_string(c.port));
    }
    return c;
}

}  // namespace safar
