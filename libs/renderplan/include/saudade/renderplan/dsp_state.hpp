#pragma once

#include <saudade/events/event_types.hpp>

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace saudade::renderplan {

/// Runtime mutable state for a sine oscillator.
/// Holds oscillator phase across block boundaries.
struct SineState {
    float phase{0.0f};
};

/// Mutable runtime state for a single polyphonic synth voice.
struct Voice {
    bool active{false};
    events::NoteId note_id{0};
    double pitch{0.0};
    double frequency{440.0};
    float velocity{0.0f};
    float phase{0.0f};
    uint64_t age{0};

    void reset() noexcept {
        active = false;
        note_id = 0;
        pitch = 0.0;
        frequency = 440.0;
        velocity = 0.0f;
        phase = 0.0f;
        age = 0;
    }
};

/// Preallocated state for a minimal polyphonic synthesizer.
/// Minimum 8 voices preallocated with zero realtime allocation.
struct PolySynthState {
    static constexpr size_t kVoiceCount = 8;

    std::array<Voice, kVoiceCount> voices{};
    uint64_t next_voice_age{0};

    void reset() noexcept {
        for (auto& v : voices) {
            v.reset();
        }
        next_voice_age = 0;
    }
};

/// Preallocated container for all mutable DSP runtime states.
/// Allocated strictly before entering the realtime processing callback.
class DspStateStorage {
public:
    DspStateStorage() = default;

    void allocate_sine_states(size_t count) {
        sine_states_.assign(count, SineState{});
    }

    void allocate_synth_states(size_t count) {
        synth_states_.assign(count, PolySynthState{});
    }

    [[nodiscard]] SineState& sine_state(size_t index) noexcept {
        return sine_states_[index];
    }

    [[nodiscard]] const SineState& sine_state(size_t index) const noexcept {
        return sine_states_[index];
    }

    [[nodiscard]] PolySynthState& synth_state(size_t index) noexcept {
        return synth_states_[index];
    }

    [[nodiscard]] const PolySynthState& synth_state(size_t index) const noexcept {
        return synth_states_[index];
    }

    void reset() noexcept {
        for (auto& s : sine_states_) {
            s.phase = 0.0f;
        }
        for (auto& s : synth_states_) {
            s.reset();
        }
    }

private:
    std::vector<SineState> sine_states_;
    std::vector<PolySynthState> synth_states_;
};

} // namespace saudade::renderplan
