#include <saudade/model/midi_import.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace saudade;

namespace {

void append_be16(std::vector<std::byte>& out, uint16_t value) {
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::byte>(value & 0xFFU));
}

void append_be32(std::vector<std::byte>& out, uint32_t value) {
    out.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::byte>(value & 0xFFU));
}

void append_tag(std::vector<std::byte>& out, const char* tag) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>(tag[i]));
}

std::vector<std::byte> make_midi() {
    std::vector<std::byte> track{
        std::byte{0x00}, std::byte{0xFF}, std::byte{0x03}, std::byte{0x04},
        std::byte{'L'}, std::byte{'e'}, std::byte{'a'}, std::byte{'d'},
        std::byte{0x00}, std::byte{0xFF}, std::byte{0x51}, std::byte{0x03},
        std::byte{0x07}, std::byte{0xA1}, std::byte{0x20}, // 120 BPM
        std::byte{0x00}, std::byte{0x90}, std::byte{60}, std::byte{100},
        std::byte{0x83}, std::byte{0x60}, // delta 480
        std::byte{0x80}, std::byte{60}, std::byte{64},
        std::byte{0x00}, std::byte{0xFF}, std::byte{0x2F}, std::byte{0x00},
    };

    std::vector<std::byte> file;
    append_tag(file, "MThd");
    append_be32(file, 6);
    append_be16(file, 0);
    append_be16(file, 1);
    append_be16(file, 480);
    append_tag(file, "MTrk");
    append_be32(file, static_cast<uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());
    return file;
}

} // namespace

int main() {
    const auto bytes = make_midi();
    std::string error;
    const auto imported = model::import_standard_midi(bytes, &error);
    assert(imported.has_value());
    assert(error.empty());
    assert(imported->initial_bpm.has_value());
    assert(std::abs(*imported->initial_bpm - 120.0) < 0.001);
    assert(imported->tracks.size() == 1);
    assert(imported->tracks.front().name == "Lead");
    assert(imported->tracks.front().notes.size() == 1);
    const auto& note = imported->tracks.front().notes.notes().front();
    assert(note.pitch == 60.0);
    assert(note.start == time::BeatPosition::zero());
    assert(note.duration == time::BeatDuration::from_beats(1));
    assert(std::abs(note.velocity - (100.0f / 127.0f)) < 0.0001f);
    assert(imported->length == time::BeatDuration::from_beats(1));

    const std::vector<std::byte> invalid{std::byte{0x01}};
    assert(!model::import_standard_midi(invalid, &error));
    assert(!error.empty());

    std::cout << "MIDI IMPORT TESTS PASSED\n";
    return 0;
}
