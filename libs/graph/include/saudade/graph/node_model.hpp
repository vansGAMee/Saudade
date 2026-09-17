#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include <cstddef>

namespace saudade::graph {

using NodeId = uint32_t;
using PortId = std::string;

struct SineNode {
    float frequency{440.0f};
    static constexpr const char* kPortOut = "out";
};

struct GainNode {
    float gain_db{-12.0f};
    static constexpr const char* kPortIn = "in";
    static constexpr const char* kPortOut = "out";
};

struct OutputNode {
    size_t channels{2};
    static constexpr const char* kPortLeft = "left";
    static constexpr const char* kPortRight = "right";
};

struct PolySynthNode {
    static constexpr const char* kPortOut = "out";
};

using NodeData = std::variant<SineNode, GainNode, OutputNode, PolySynthNode>;

struct NodeRecord {
    NodeId id{0};
    NodeData data;
};

struct Connection {
    NodeId from_node{0};
    PortId from_port;
    NodeId to_node{0};
    PortId to_port;

    bool operator==(const Connection& other) const noexcept {
        return from_node == other.from_node &&
               from_port == other.from_port &&
               to_node == other.to_node &&
               to_port == other.to_port;
    }
};

} // namespace saudade::graph
