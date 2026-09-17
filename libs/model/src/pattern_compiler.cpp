#include <saudade/model/pattern_compiler.hpp>
#include <algorithm>

namespace saudade::model {

std::vector<events::TimelineEvent> PatternCompiler::compile(
    const Pattern& pattern,
    const time::TempoMap& tempo_map,
    double sample_rate,
    time::BeatPosition pattern_start_beat) {

    size_t total_notes = 0;
    for (const auto& lane : pattern.lanes()) {
        total_notes += lane.notes().size();
    }

    std::vector<events::TimelineEvent> events;
    events.reserve(total_notes * 2);

    for (const auto& lane : pattern.lanes()) {
        for (const auto& note : lane.notes().notes()) {
            if (!note.is_valid()) {
                continue;
            }

            const auto note_start_beat = pattern_start_beat + time::BeatDuration::from_ticks(note.start.ticks);
            const auto note_end_beat = note_start_beat + note.duration;

            const auto start_sample = tempo_map.beat_to_sample(note_start_beat, sample_rate);
            const auto end_sample = tempo_map.beat_to_sample(note_end_beat, sample_rate);

            events.emplace_back(start_sample, events::NoteOn{note.note_id, note.pitch, note.velocity});
            events.emplace_back(end_sample, events::NoteOff{note.note_id, note.release_velocity});
        }
    }

    std::sort(events.begin(), events.end());
    return events;
}

std::vector<events::TimelineEvent> PatternCompiler::compile_sequence(
    const NoteSequence& sequence,
    const time::TempoMap& tempo_map,
    double sample_rate,
    time::BeatPosition start_beat) {

    std::vector<events::TimelineEvent> events;
    events.reserve(sequence.size() * 2);

    for (const auto& note : sequence.notes()) {
        if (!note.is_valid()) {
            continue;
        }

        const auto note_start_beat = start_beat + time::BeatDuration::from_ticks(note.start.ticks);
        const auto note_end_beat = note_start_beat + note.duration;

        const auto start_sample = tempo_map.beat_to_sample(note_start_beat, sample_rate);
        const auto end_sample = tempo_map.beat_to_sample(note_end_beat, sample_rate);

        events.emplace_back(start_sample, events::NoteOn{note.note_id, note.pitch, note.velocity});
        events.emplace_back(end_sample, events::NoteOff{note.note_id, note.release_velocity});
    }

    std::sort(events.begin(), events.end());
    return events;
}

} // namespace saudade::model
