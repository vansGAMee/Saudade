#pragma once

#include <saudade/time/time_types.hpp>
#include <cstdint>

namespace saudade::audio {

/// Dynamic execution context passed to realtime processing for each block.
/// Provides sample rate, block size, and immutable timeline transport information.
struct ProcessContext {
    double sample_rate{48000.0};
    uint32_t num_frames{0};
    time::SamplePosition block_start_sample{0};
    bool transport_playing{false};
};

} // namespace saudade::audio
