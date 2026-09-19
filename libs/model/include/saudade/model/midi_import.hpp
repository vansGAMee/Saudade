#pragma once

#include <saudade/model/note_sequence.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace saudade::model {

struct ImportedMidiTrack {
    std::string name;
    NoteSequence notes;
};

struct ImportedMidi {
    std::optional<double> initial_bpm;
    std::vector<ImportedMidiTrack> tracks;
    time::BeatDuration length{};
};

/// Parses a Standard MIDI File from memory into canonical musical data.
/// Supports format 0/1, PPQN timing, running status, note events, track names,
/// and the initial tempo. SMPTE division is rejected explicitly.
std::optional<ImportedMidi> import_standard_midi(
    std::span<const std::byte> bytes, std::string* error = nullptr);

} // namespace saudade::model
