#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace saudade::renderplan {

/// Runtime mutable state for a sine oscillator.
/// Holds oscillator phase across block boundaries.
struct SineState {
    float phase{0.0f};
};

/// Preallocated container for all mutable DSP runtime states.
/// Allocated strictly before entering the realtime processing callback.
class DspStateStorage {
public:
    DspStateStorage() = default;

    void allocate_sine_states(size_t count) {
        sine_states_.assign(count, SineState{});
    }

    [[nodiscard]] SineState& sine_state(size_t index) noexcept {
        return sine_states_[index];
    }

    [[nodiscard]] const SineState& sine_state(size_t index) const noexcept {
        return sine_states_[index];
    }

    void reset() noexcept {
        for (auto& s : sine_states_) {
            s.phase = 0.0f;
        }
    }

private:
    std::vector<SineState> sine_states_;
};

} // namespace saudade::renderplan
