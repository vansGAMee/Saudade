#pragma once

#include <saudade/model/pattern.hpp>
#include <saudade/model/note_sequence.hpp>
#include <saudade/time/tempo_map.hpp>
#include <saudade/time/time_types.hpp>
#include <saudade/events/event_types.hpp>

#include <vector>

namespace saudade::model {

/// Control-side compiler that converts high-level musical patterns and note sequences
/// into sample-accurate, deterministically ordered TimelineEvents.
class PatternCompiler {
public:
    /// Compiles all lanes of a Pattern into a deterministically ordered vector of TimelineEvents.
    [[nodiscard]] static std::vector<events::TimelineEvent> compile(
        const Pattern& pattern,
        const time::TempoMap& tempo_map,
        double sample_rate,
        time::BeatPosition pattern_start_beat = time::BeatPosition::zero());

    /// Compiles a single NoteSequence into a deterministically ordered vector of TimelineEvents.
    [[nodiscard]] static std::vector<events::TimelineEvent> compile_sequence(
        const NoteSequence& sequence,
        const time::TempoMap& tempo_map,
        double sample_rate,
        time::BeatPosition start_beat = time::BeatPosition::zero());
};

} // namespace saudade::model
