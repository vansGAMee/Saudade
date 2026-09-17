#pragma once

#include <cstdint>
#include <variant>

namespace saudade::renderplan {

/// Parameters for a sine oscillator execution step.
struct SineStep {
    float frequency{440.0f};
    uint32_t output_buffer_slot{0};
    uint32_t state_index{0};
};

/// Parameters for a gain execution step.
/// Linear gain is computed beforehand during compilation (outside RT callback).
struct GainStep {
    float linear_gain{1.0f};
    uint32_t input_buffer_slot{0};
    uint32_t output_buffer_slot{0};
};

/// Parameters for routing an internal buffer slot to an output endpoint channel.
struct RouteStep {
    uint32_t source_buffer_slot{0};
    uint32_t destination_channel{0};
};

/// Parameters for a minimal polyphonic synthesizer execution step.
struct PolySynthStep {
    uint32_t output_buffer_slot{0};
    uint32_t state_index{0};
};

using ExecutionStep = std::variant<SineStep, GainStep, RouteStep, PolySynthStep>;

} // namespace saudade::renderplan
