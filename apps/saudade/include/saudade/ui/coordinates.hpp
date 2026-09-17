#pragma once

#include <saudade/time/time_types.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <algorithm>

namespace saudade::ui {

/// Piano Roll spatial and musical coordinate conversion helpers.
/// Keeps pure mathematical transformations isolated from GUI renderers and widgets.
struct Coordinates {
    static constexpr int kDefaultMinPitch = 48; // C3
    static constexpr int kDefaultMaxPitch = 84; // C6
    static constexpr float kDefaultRowHeight = 20.0f;
    static constexpr float kDefaultBeatWidth = 160.0f;

    /// Converts musical BeatPosition to horizontal pixel coordinate X.
    [[nodiscard]] static constexpr float beat_to_x(time::BeatPosition beat, float beat_width = kDefaultBeatWidth) noexcept {
        return static_cast<float>(beat.to_double() * static_cast<double>(beat_width));
    }

    /// Converts horizontal pixel coordinate X to musical BeatPosition.
    [[nodiscard]] static time::BeatPosition x_to_beat(float x, float beat_width = kDefaultBeatWidth) noexcept {
        if (x <= 0.0f || beat_width <= 0.0f) {
            return time::BeatPosition::zero();
        }
        const double beats = static_cast<double>(x) / static_cast<double>(beat_width);
        const auto ticks = static_cast<int64_t>(std::llround(beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
        return time::BeatPosition::from_ticks(std::max<int64_t>(0, ticks));
    }

    /// Converts musical pitch (semitones) to vertical pixel coordinate Y.
    /// Higher pitch is at the top (smaller Y), lower pitch is at the bottom (larger Y).
    [[nodiscard]] static float pitch_to_y(double pitch,
                                          float row_height = kDefaultRowHeight,
                                          int max_pitch = kDefaultMaxPitch) noexcept {
        return static_cast<float>((static_cast<double>(max_pitch) - pitch) * static_cast<double>(row_height));
    }

    /// Converts vertical pixel coordinate Y to fractional semitone pitch.
    [[nodiscard]] static double y_to_pitch(float y,
                                           float row_height = kDefaultRowHeight,
                                           int max_pitch = kDefaultMaxPitch) noexcept {
        if (row_height <= 0.0f) {
            return static_cast<double>(max_pitch);
        }
        return static_cast<double>(max_pitch) - (static_cast<double>(y) / static_cast<double>(row_height));
    }

    /// Quantizes pixel Y coordinate to discrete integer semitone pitch within [min_pitch, max_pitch].
    [[nodiscard]] static int quantize_pitch(float y,
                                            float row_height = kDefaultRowHeight,
                                            int min_pitch = kDefaultMinPitch,
                                            int max_pitch = kDefaultMaxPitch) noexcept {
        if (row_height <= 0.0f) {
            return min_pitch;
        }
        const int row = static_cast<int>(std::floor(static_cast<double>(y) / static_cast<double>(row_height)));
        const int pitch = max_pitch - row;
        return std::clamp(pitch, min_pitch, max_pitch);
    }

    /// Quantizes BeatPosition to the nearest grid subdivision (e.g. 1/4 beat).
    [[nodiscard]] static time::BeatPosition quantize_beat(
        time::BeatPosition beat,
        time::BeatDuration grid_step = time::BeatDuration::from_fraction(1, 4)) noexcept {

        const int64_t step_ticks = grid_step.ticks;
        if (step_ticks <= 0) {
            return beat;
        }
        const int64_t half = step_ticks / 2;
        const int64_t q_ticks = ((beat.ticks + half) / step_ticks) * step_ticks;
        return time::BeatPosition::from_ticks(std::max<int64_t>(0, q_ticks));
    }

    /// Floors BeatPosition to the grid subdivision cell (for click detection on grid cells).
    [[nodiscard]] static time::BeatPosition floor_quantize_beat(
        time::BeatPosition beat,
        time::BeatDuration grid_step = time::BeatDuration::from_fraction(1, 4)) noexcept {

        const int64_t step_ticks = grid_step.ticks;
        if (step_ticks <= 0) {
            return beat;
        }
        const int64_t q_ticks = (beat.ticks / step_ticks) * step_ticks;
        return time::BeatPosition::from_ticks(std::max<int64_t>(0, q_ticks));
    }

    /// Quantizes BeatDuration to grid step with a minimum bound of 1 step.
    [[nodiscard]] static time::BeatDuration quantize_duration(
        time::BeatDuration dur,
        time::BeatDuration min_step = time::BeatDuration::from_fraction(1, 4)) noexcept {

        const int64_t step_ticks = min_step.ticks;
        if (step_ticks <= 0) {
            return dur.ticks > 0 ? dur : time::BeatDuration::from_fraction(1, 4);
        }
        const int64_t half = step_ticks / 2;
        int64_t q_ticks = ((dur.ticks + half) / step_ticks) * step_ticks;
        if (q_ticks < step_ticks) {
            q_ticks = step_ticks;
        }
        return time::BeatDuration::from_ticks(q_ticks);
    }

    /// Checks if a semitone pitch corresponds to an accidental (black key: C#, D#, F#, G#, A#).
    [[nodiscard]] static constexpr bool is_black_key(int pitch) noexcept {
        const int m = ((pitch % 12) + 12) % 12;
        return m == 1 || m == 3 || m == 6 || m == 8 || m == 10;
    }

    /// Returns the scientific pitch notation (e.g. "C4", "F#3", "A5").
    [[nodiscard]] static std::string pitch_name(int pitch) {
        static constexpr const char* kNoteNames[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int m = ((pitch % 12) + 12) % 12;
        const int octave = (pitch / 12) - 1;
        return std::string(kNoteNames[m]) + std::to_string(octave);
    }
};

} // namespace saudade::ui
