#pragma once

#include <filesystem>
#include <string>

namespace safar {

struct Config {
    std::string host = "0.0.0.0";
    int port = 8080;
    std::filesystem::path data_dir = "../data/processed";
    std::string cors_origin = "http://localhost:5173";
    std::string log_level = "info";
    std::filesystem::path transport_file = "config/transport.json";  // fares, road modes, traffic
    std::filesystem::path providers_file;  // Uber/Ola/Rapido/meter rate cards; empty = next to transport_file

    std::filesystem::path providers_path() const {
        return providers_file.empty() ? transport_file.parent_path() / "providers.json" : providers_file;
    }

    // Reads a JSON config file (missing keys keep their defaults), then applies
    // SAFAR_HOST / SAFAR_PORT / SAFAR_DATA_DIR / SAFAR_CORS_ORIGIN / SAFAR_LOG_LEVEL,
    // SAFAR_TRANSPORT_FILE / SAFAR_PROVIDERS_FILE.
    static Config load(const std::filesystem::path& file);
};

}  // namespace safar
