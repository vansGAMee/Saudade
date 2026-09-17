#pragma once

#include <saudade/model/note.hpp>

#include <vector>
#include <optional>
#include <cstddef>

namespace saudade::model {

/// Editable, ordered sequence of musical notes.
/// Application-level control-side model with identity preservation and invariant validation.
class NoteSequence {
public:
    NoteSequence() = default;

    /// Adds a fully-formed Note.
    /// Rejects invalid attributes or duplicate NoteId.
    bool add_note(const Note& note);

    /// Convenience overload allocating a unique monotonic NoteId.
    std::optional<events::NoteId> add_note(time::BeatPosition start,
                                           time::BeatDuration duration,
                                           double pitch,
                                           float velocity = 0.8f,
                                           float release_velocity = 0.0f);

    /// Removes a note by its stable NoteId.
    bool remove_note(events::NoteId id) noexcept;

    /// Moves a note's start position. Rejects negative start.
    bool move_note(events::NoteId id, time::BeatPosition new_start) noexcept;

    /// Resizes a note's duration. Rejects non-positive duration.
    bool resize_note(events::NoteId id, time::BeatDuration new_duration) noexcept;

    /// Updates pitch. Rejects non-finite values.
    bool update_pitch(events::NoteId id, double new_pitch) noexcept;

    /// Updates velocity values. Rejects values outside [0.0, 1.0].
    bool update_velocity(events::NoteId id, float new_velocity, float new_release_velocity = 0.0f) noexcept;

    /// Replaces note attributes wholesale while maintaining validation invariants.
    bool update_note(const Note& updated);

    [[nodiscard]] const Note* find_note(events::NoteId id) const noexcept;
    [[nodiscard]] Note* find_note(events::NoteId id) noexcept;

    [[nodiscard]] const std::vector<Note>& notes() const noexcept { return notes_; }
    [[nodiscard]] size_t size() const noexcept { return notes_.size(); }
    [[nodiscard]] bool empty() const noexcept { return notes_.empty(); }

    void clear() noexcept;

private:
    std::vector<Note> notes_;
    events::NoteId next_note_id_{1};
};

} // namespace saudade::model
