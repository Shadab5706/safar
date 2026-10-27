#include "app/context.hpp"

namespace safar {

AppContext AppContext::load(const Config& config) {
    AppContext ctx;
    ctx.dataset = std::make_shared<const data::Dataset>(data::Dataset::load(config.data_dir));
    ctx.transport = std::make_shared<const fares::TransportConfig>(fares::TransportConfig::load(config.transport_file));
    ctx.roads = std::make_shared<const roads::RoadMatrix>(roads::RoadMatrix::load(config.data_dir / "road_matrix.json"));
    // Building the graph with the fare config also checks every line has a fare table.
    ctx.transit = std::make_shared<const graph::Graph>(graph::build_transit_graph(*ctx.dataset, {}, ctx.transport.get()));
    ctx.fares = std::make_shared<const fares::FareEngine>(fares::FareEngine::load(config.providers_path(), ctx.transport));
    ctx.planner = std::make_shared<const journey::Planner>(ctx.dataset, ctx.transport, ctx.roads, ctx.transit, ctx.fares);
    return ctx;
}

}  // namespace safar
