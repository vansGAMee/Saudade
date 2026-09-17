#include <saudade/model/note_sequence.hpp>
#include <algorithm>

namespace saudade::model {

bool NoteSequence::add_note(const Note& note) {
    if (!note.is_valid()) {
        return false;
    }

    // Check NoteId uniqueness
    if (find_note(note.note_id) != nullptr) {
        return false;
    }

    notes_.push_back(note);
    if (note.note_id >= next_note_id_) {
        next_note_id_ = note.note_id + 1;
    }
    return true;
}

std::optional<events::NoteId> NoteSequence::add_note(time::BeatPosition start,
                                                     time::BeatDuration duration,
                                                     double pitch,
                                                     float velocity,
                                                     float release_velocity) {
    const events::NoteId id = next_note_id_++;
    Note n{
        .note_id = id,
        .start = start,
        .duration = duration,
        .pitch = pitch,
        .velocity = velocity,
        .release_velocity = release_velocity
    };

    if (!n.is_valid()) {
        return std::nullopt;
    }

    notes_.push_back(n);
    return id;
}

bool NoteSequence::remove_note(events::NoteId id) noexcept {
    const auto it = std::find_if(notes_.begin(), notes_.end(), [id](const Note& n) {
        return n.note_id == id;
    });

    if (it == notes_.end()) {
        return false;
    }

    notes_.erase(it);
    return true;
}

bool NoteSequence::move_note(events::NoteId id, time::BeatPosition new_start) noexcept {
    if (new_start.ticks < 0) {
        return false;
    }

    Note* note = find_note(id);
    if (!note) {
        return false;
    }

    note->start = new_start;
    return true;
}

bool NoteSequence::resize_note(events::NoteId id, time::BeatDuration new_duration) noexcept {
    if (new_duration.ticks <= 0) {
        return false;
    }

    Note* note = find_note(id);
    if (!note) {
        return false;
    }

    note->duration = new_duration;
    return true;
}

bool NoteSequence::update_pitch(events::NoteId id, double new_pitch) noexcept {
    if (!std::isfinite(new_pitch)) {
        return false;
    }

    Note* note = find_note(id);
    if (!note) {
        return false;
    }

    note->pitch = new_pitch;
    return true;
}

bool NoteSequence::update_velocity(events::NoteId id, float new_velocity, float new_release_velocity) noexcept {
    if (new_velocity < 0.0f || new_velocity > 1.0f ||
        new_release_velocity < 0.0f || new_release_velocity > 1.0f) {
        return false;
    }

    Note* note = find_note(id);
    if (!note) {
        return false;
    }

    note->velocity = new_velocity;
    note->release_velocity = new_release_velocity;
    return true;
}

bool NoteSequence::update_note(const Note& updated) {
    if (!updated.is_valid()) {
        return false;
    }

    Note* note = find_note(updated.note_id);
    if (!note) {
        return false;
    }

    *note = updated;
    return true;
}

const Note* NoteSequence::find_note(events::NoteId id) const noexcept {
    for (const auto& n : notes_) {
        if (n.note_id == id) {
            return &n;
        }
    }
    return nullptr;
}

Note* NoteSequence::find_note(events::NoteId id) noexcept {
    for (auto& n : notes_) {
        if (n.note_id == id) {
            return &n;
        }
    }
    return nullptr;
}

void NoteSequence::clear() noexcept {
    notes_.clear();
}

} // namespace saudade::model
