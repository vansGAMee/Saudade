#pragma once

#include <saudade/graph/node_model.hpp>
#include <unordered_map>
#include <vector>
#include <optional>

namespace saudade::graph {

/// Editable DAG representation of the audio graph.
/// Never traversed or executed directly by the realtime audio callback.
class GraphModel {
public:
    GraphModel() = default;

    NodeId add_sine_node(float frequency = 440.0f);
    NodeId add_gain_node(float gain_db = -12.0f);
    NodeId add_output_node(size_t channels = 2);

    bool connect(NodeId from_node, PortId from_port, NodeId to_node, PortId to_port);

    [[nodiscard]] const NodeRecord* find_node(NodeId id) const noexcept;
    [[nodiscard]] const std::unordered_map<NodeId, NodeRecord>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Connection>& connections() const noexcept { return connections_; }

private:
    NodeId next_id_{1};
    std::unordered_map<NodeId, NodeRecord> nodes_;
    std::vector<Connection> connections_;
};

} // namespace saudade::graph
