#include <csignal>
#include <cstdlib>
#include <exception>

#include <spdlog/spdlog.h>

#include "api/server.hpp"
#include "util/config.hpp"

namespace {
safar::api::Server* g_server = nullptr;

void on_signal(int) {
    if (g_server) g_server->stop();
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const char* config_path = argc > 1 ? argv[1] : "config/server.json";
        auto config = safar::Config::load(config_path);
        spdlog::set_level(spdlog::level::from_str(config.log_level));
        spdlog::info("data dir: {}", config.data_dir.string());

        auto dataset = std::make_shared<const safar::data::Dataset>(safar::data::Dataset::load(config.data_dir));
        spdlog::info("loaded {} lines, {} stations, {} segments, {} places", dataset->lines().size(),
                     dataset->stations().size(), dataset->segments().size(), dataset->places().size());

        safar::api::Server server(std::move(config), std::move(dataset));
        g_server = &server;
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        if (!server.listen()) {
            spdlog::critical("could not bind to the configured host/port");
            return EXIT_FAILURE;
        }
        spdlog::info("shut down cleanly");
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        spdlog::critical("startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
