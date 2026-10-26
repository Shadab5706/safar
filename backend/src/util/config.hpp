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

    // Reads a JSON config file (missing keys keep their defaults), then applies
    // SAFAR_HOST / SAFAR_PORT / SAFAR_DATA_DIR / SAFAR_CORS_ORIGIN / SAFAR_LOG_LEVEL.
    static Config load(const std::filesystem::path& file);
};

}  // namespace safar
