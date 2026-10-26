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

        auto ctx = safar::AppContext::load(config);
        const auto& ds = *ctx.dataset;
        spdlog::info("loaded {} lines, {} stations, {} segments, {} places", ds.lines().size(), ds.stations().size(),
                     ds.segments().size(), ds.places().size());
        spdlog::info("transit graph: {} nodes, {} edges, heuristic speed {:.0f} km/h", ctx.transit->node_count(),
                     ctx.transit->edge_count(), ctx.transit->max_speed_kmh());
        if (ctx.roads->size() == 0) {
            spdlog::warn("no road_matrix.json: road times are straight-line estimates (run scripts/prefetch_road_matrix.py)");
        } else {
            spdlog::info("road matrix: {} OSRM pairs", ctx.roads->size());
        }

        safar::api::Server server(std::move(config), std::move(ctx));
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
