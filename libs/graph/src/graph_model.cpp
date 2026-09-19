#include <saudade/graph/graph_model.hpp>
#include <algorithm>

namespace saudade::graph {

NodeId GraphModel::add_sine_node(float frequency) {
    const NodeId id = next_id_++;
    nodes_[id] = NodeRecord{id, SineNode{frequency}};
    return id;
}

NodeId GraphModel::add_poly_synth_node() {
    return add_poly_synth_node(PolySynthNode{});
}

NodeId GraphModel::add_poly_synth_node(const PolySynthNode& patch) {
    const NodeId id = next_id_++;
    nodes_[id] = NodeRecord{id, patch};
    return id;
}

NodeId GraphModel::add_gain_node(float gain_db) {
    const NodeId id = next_id_++;
    nodes_[id] = NodeRecord{id, GainNode{gain_db}};
    return id;
}

NodeId GraphModel::add_output_node(size_t channels) {
    const NodeId id = next_id_++;
    nodes_[id] = NodeRecord{id, OutputNode{channels}};
    return id;
}

bool GraphModel::connect(NodeId from_node, PortId from_port, NodeId to_node, PortId to_port) {
    if (!nodes_.contains(from_node) || !nodes_.contains(to_node)) {
        return false;
    }

    Connection conn{from_node, std::move(from_port), to_node, std::move(to_port)};
    if (std::find(connections_.begin(), connections_.end(), conn) != connections_.end()) {
        return false; // Already connected
    }

    connections_.push_back(std::move(conn));
    return true;
}

const NodeRecord* GraphModel::find_node(NodeId id) const noexcept {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace saudade::graph
