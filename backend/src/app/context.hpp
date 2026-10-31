#pragma once

#include <memory>

#include "assistant/assistant.hpp"
#include "data/dataset.hpp"
#include "fares/fare_engine.hpp"
#include "fares/transport_config.hpp"
#include "graph/graph.hpp"
#include "journey/planner.hpp"
#include "roads/road_matrix.hpp"
#include "util/config.hpp"

namespace safar {

// Everything loaded once at startup and shared read-only by all request threads.
struct AppContext {
    std::shared_ptr<const data::Dataset> dataset;
    std::shared_ptr<const fares::TransportConfig> transport;
    std::shared_ptr<const roads::RoadMatrix> roads;
    std::shared_ptr<const graph::Graph> transit;
    std::shared_ptr<const fares::FareEngine> fares;
    std::shared_ptr<const journey::Planner> planner;
    std::shared_ptr<const assistant::Assistant> assistant;

    // Throws with a clear message if any file is missing or inconsistent.
    static AppContext load(const Config& config);
};

}  // namespace safar
