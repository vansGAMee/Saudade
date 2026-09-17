#pragma once

#include <saudade/time/time_types.hpp>
#include <saudade/events/event_types.hpp>

#include <cmath>
#include <compare>

namespace saudade::model {

/// Canonical musical note representation within a NoteSequence.
/// Holds musical timeline coordinates (BeatPosition and BeatDuration),
/// fractional semitone pitch (60.0 = C4, 69.0 = A4), and normalized velocities.
struct Note {
    events::NoteId note_id{0};
    time::BeatPosition start{time::BeatPosition::zero()};
    time::BeatDuration duration{time::BeatDuration::from_fraction(1, 4)}; // 1 quarter note default
    double pitch{60.0};
    float velocity{0.8f};
    float release_velocity{0.0f};

    /// Checks that note attributes adhere strictly to model invariants:
    /// - note_id != 0
    /// - start >= 0
    /// - duration > 0
    /// - pitch is finite
    /// - velocity and release_velocity are bounded within [0.0, 1.0]
    [[nodiscard]] bool is_valid() const noexcept {
        return note_id != 0 &&
               start.ticks >= 0 &&
               duration.ticks > 0 &&
               std::isfinite(pitch) &&
               velocity >= 0.0f && velocity <= 1.0f &&
               release_velocity >= 0.0f && release_velocity <= 1.0f;
    }

    auto operator<=>(const Note& other) const noexcept = default;
    bool operator==(const Note& other) const noexcept = default;
};

} // namespace saudade::model
