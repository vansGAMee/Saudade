#pragma once

#include <cstdint>

namespace saudade::audio {

/// Dynamic execution context passed to realtime processing for each block.
/// Sample rate and frame count are determined at runtime (e.g. negotiated with PipeWire
/// or configured explicitly in offline tests) and are never compile-time constants.
struct ProcessContext {
    double sample_rate{48000.0};
    uint32_t num_frames{0};
};

} // namespace saudade::audio
