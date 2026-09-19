#include <saudade/graph/graph_compiler.hpp>
#include <cmath>
#include <queue>
#include <map>
#include <set>

namespace saudade::graph {

namespace {

float db_to_linear(float gain_db) noexcept {
    return std::pow(10.0f, gain_db / 20.0f);
}

} // namespace

std::unique_ptr<renderplan::RenderPlan> GraphCompiler::compile(const GraphModel& graph) {
    const auto& nodes = graph.nodes();
    const auto& connections = graph.connections();

    if (nodes.empty()) {
        throw GraphCompilationException("Graph cannot be compiled: graph is empty");
    }

    // 1. Validate Output node presence (must have exactly 1)
    size_t output_node_count = 0;
    for (const auto& [id, record] : nodes) {
        (void)id;
        if (std::holds_alternative<OutputNode>(record.data)) {
            ++output_node_count;
        }
    }
    if (output_node_count != 1) {
        throw GraphCompilationException("Graph must contain exactly one Output node");
    }

    // 2. Validate all connections (port existence and node existence)
    std::map<std::pair<NodeId, PortId>, Connection> input_port_connections;

    for (const auto& conn : connections) {
        const auto from_it = nodes.find(conn.from_node);
        const auto to_it = nodes.find(conn.to_node);

        if (from_it == nodes.end()) {
            throw GraphCompilationException("Connection refers to non-existent source node");
        }
        if (to_it == nodes.end()) {
            throw GraphCompilationException("Connection refers to non-existent destination node");
        }

        // Validate source port
        std::visit([&](const auto& src_node) {
            using T = std::decay_t<decltype(src_node)>;
            if constexpr (std::is_same_v<T, SineNode>) {
                if (conn.from_port != SineNode::kPortOut) {
                    throw GraphCompilationException("Invalid output port for SineNode: " + conn.from_port);
                }
            } else if constexpr (std::is_same_v<T, PolySynthNode>) {
                if (conn.from_port != PolySynthNode::kPortOut &&
                    conn.from_port != PolySynthNode::kPortLeft &&
                    conn.from_port != PolySynthNode::kPortRight) {
                    throw GraphCompilationException("Invalid output port for PolySynthNode: " + conn.from_port);
                }
            } else if constexpr (std::is_same_v<T, GainNode>) {
                if (conn.from_port != GainNode::kPortOut) {
                    throw GraphCompilationException("Invalid output port for GainNode: " + conn.from_port);
                }
            } else if constexpr (std::is_same_v<T, OutputNode>) {
                throw GraphCompilationException("OutputNode cannot be a connection source");
            }
        }, from_it->second.data);

        // Validate destination port
        std::visit([&](const auto& dst_node) {
            using T = std::decay_t<decltype(dst_node)>;
            if constexpr (std::is_same_v<T, SineNode>) {
                throw GraphCompilationException("SineNode has no input ports");
            } else if constexpr (std::is_same_v<T, PolySynthNode>) {
                throw GraphCompilationException("PolySynthNode has no input ports");
            } else if constexpr (std::is_same_v<T, GainNode>) {
                if (conn.to_port != GainNode::kPortIn) {
                    throw GraphCompilationException("Invalid input port for GainNode: " + conn.to_port);
                }
            } else if constexpr (std::is_same_v<T, OutputNode>) {
                if (conn.to_port != OutputNode::kPortLeft && conn.to_port != OutputNode::kPortRight) {
                    throw GraphCompilationException("Invalid input port for OutputNode: " + conn.to_port);
                }
            }
        }, to_it->second.data);

        // Check for multiple inputs to the same destination port
        const auto key = std::make_pair(conn.to_node, conn.to_port);
        if (input_port_connections.contains(key)) {
            throw GraphCompilationException("Input port connected multiple times: node " +
                                            std::to_string(conn.to_node) + " port " + conn.to_port);
        }
        input_port_connections[key] = conn;
    }

    // 3. Verify required inputs are connected
    for (const auto& [id, record] : nodes) {
        if (std::holds_alternative<GainNode>(record.data)) {
            if (!input_port_connections.contains({id, GainNode::kPortIn})) {
                throw GraphCompilationException("GainNode input port is not connected");
            }
        } else if (std::holds_alternative<OutputNode>(record.data)) {
            if (!input_port_connections.contains({id, OutputNode::kPortLeft})) {
                throw GraphCompilationException("OutputNode left port is not connected");
            }
            if (!input_port_connections.contains({id, OutputNode::kPortRight})) {
                throw GraphCompilationException("OutputNode right port is not connected");
            }
        }
    }

    // 4. Cycle detection and topological ordering (Kahn's algorithm)
    std::map<NodeId, std::vector<NodeId>> adj;
    std::map<NodeId, size_t> in_degree;

    for (const auto& [id, _] : nodes) {
        adj[id] = {};
        in_degree[id] = 0;
    }

    for (const auto& conn : connections) {
        adj[conn.from_node].push_back(conn.to_node);
        ++in_degree[conn.to_node];
    }

    std::queue<NodeId> zero_in_degree_queue;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) {
            zero_in_degree_queue.push(id);
        }
    }

    std::vector<NodeId> execution_order;
    while (!zero_in_degree_queue.empty()) {
        const NodeId u = zero_in_degree_queue.front();
        zero_in_degree_queue.pop();
        execution_order.push_back(u);

        for (const NodeId v : adj[u]) {
            if (--in_degree[v] == 0) {
                zero_in_degree_queue.push(v);
            }
        }
    }

    if (execution_order.size() != nodes.size()) {
        throw GraphCompilationException("Graph contains cycles or unreachable components");
    }

    // 5. Buffer slot allocation and step generation
    std::map<NodeId, uint32_t> node_output_slots;
    std::map<NodeId, uint32_t> synth_right_output_slots;
    uint32_t next_scratch_slot = 0;
    size_t next_sine_state = 0;
    size_t next_synth_state = 0;
    std::vector<renderplan::ExecutionStep> steps;

    for (const NodeId id : execution_order) {
        const auto& record = nodes.at(id);

        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, SineNode>) {
                const uint32_t out_slot = next_scratch_slot++;
                node_output_slots[id] = out_slot;

                const uint32_t state_idx = static_cast<uint32_t>(next_sine_state++);
                steps.push_back(renderplan::SineStep{
                    .frequency = node.frequency,
                    .output_buffer_slot = out_slot,
                    .state_index = state_idx
                });
            } else if constexpr (std::is_same_v<T, PolySynthNode>) {
                const uint32_t left_slot = next_scratch_slot++;
                const uint32_t right_slot = next_scratch_slot++;
                node_output_slots[id] = left_slot;
                synth_right_output_slots[id] = right_slot;

                const uint32_t state_idx = static_cast<uint32_t>(next_synth_state++);
                steps.push_back(renderplan::PolySynthStep{
                    .output_left_buffer_slot = left_slot,
                    .output_right_buffer_slot = right_slot,
                    .state_index = state_idx,
                    .attack_seconds = std::clamp(node.attack_seconds, 0.001f, 2.0f),
                    .decay_seconds = std::clamp(node.decay_seconds, 0.005f, 4.0f),
                    .sustain = std::clamp(node.sustain, 0.0f, 1.0f),
                    .release_seconds = std::clamp(node.release_seconds, 0.005f, 6.0f),
                    .cutoff_hz = std::clamp(node.cutoff_hz, 40.0f, 18000.0f),
                    .resonance = std::clamp(node.resonance, 0.0f, 0.95f),
                    .character = std::clamp(node.character, 0.0f, 1.0f)
                });
            } else if constexpr (std::is_same_v<T, GainNode>) {
                const auto& in_conn = input_port_connections.at({id, GainNode::kPortIn});
                uint32_t in_slot = node_output_slots.at(in_conn.from_node);
                if (in_conn.from_port == PolySynthNode::kPortRight) {
                    in_slot = synth_right_output_slots.at(in_conn.from_node);
                }

                const uint32_t out_slot = next_scratch_slot++;
                node_output_slots[id] = out_slot;

                const float linear_gain = db_to_linear(node.gain_db);
                steps.push_back(renderplan::GainStep{
                    .linear_gain = linear_gain,
                    .input_buffer_slot = in_slot,
                    .output_buffer_slot = out_slot
                });
            } else if constexpr (std::is_same_v<T, OutputNode>) {
                const auto& conn_l = input_port_connections.at({id, OutputNode::kPortLeft});
                const auto& conn_r = input_port_connections.at({id, OutputNode::kPortRight});

                uint32_t slot_l = node_output_slots.at(conn_l.from_node);
                uint32_t slot_r = node_output_slots.at(conn_r.from_node);
                if (conn_l.from_port == PolySynthNode::kPortRight) {
                    slot_l = synth_right_output_slots.at(conn_l.from_node);
                }
                if (conn_r.from_port == PolySynthNode::kPortRight) {
                    slot_r = synth_right_output_slots.at(conn_r.from_node);
                }

                steps.push_back(renderplan::RouteStep{
                    .source_buffer_slot = slot_l,
                    .destination_channel = 0
                });
                steps.push_back(renderplan::RouteStep{
                    .source_buffer_slot = slot_r,
                    .destination_channel = 1
                });
            }
        }, record.data);
    }

    return std::make_unique<renderplan::RenderPlan>(
        std::move(steps),
        next_scratch_slot,
        next_sine_state,
        /*output_channels=*/2,
        next_synth_state
    );
}

} // namespace saudade::graph
