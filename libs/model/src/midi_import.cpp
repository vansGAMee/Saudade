#include <saudade/model/midi_import.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace saudade::model {
namespace {

class Reader {
public:
    explicit Reader(std::span<const std::byte> data) : data_(data) {}

    [[nodiscard]] size_t remaining() const noexcept { return data_.size() - pos_; }
    [[nodiscard]] size_t position() const noexcept { return pos_; }

    bool read_u8(uint8_t& value) {
        if (remaining() < 1) return false;
        value = std::to_integer<uint8_t>(data_[pos_++]);
        return true;
    }

    bool read_be16(uint16_t& value) {
        uint8_t a = 0;
        uint8_t b = 0;
        if (!read_u8(a) || !read_u8(b)) return false;
        value = static_cast<uint16_t>((static_cast<uint16_t>(a) << 8U) | b);
        return true;
    }

    bool read_be32(uint32_t& value) {
        uint8_t a = 0;
        uint8_t b = 0;
        uint8_t c = 0;
        uint8_t d = 0;
        if (!read_u8(a) || !read_u8(b) || !read_u8(c) || !read_u8(d)) return false;
        value = (static_cast<uint32_t>(a) << 24U) |
                (static_cast<uint32_t>(b) << 16U) |
                (static_cast<uint32_t>(c) << 8U) |
                static_cast<uint32_t>(d);
        return true;
    }

    bool read_vlq(uint32_t& value) {
        value = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = 0;
            if (!read_u8(byte)) return false;
            value = (value << 7U) | (byte & 0x7FU);
            if ((byte & 0x80U) == 0) return true;
        }
        return false;
    }

    bool skip(size_t count) {
        if (remaining() < count) return false;
        pos_ += count;
        return true;
    }

    std::span<const std::byte> take(size_t count) {
        if (remaining() < count) return {};
        const auto result = data_.subspan(pos_, count);
        pos_ += count;
        return result;
    }

private:
    std::span<const std::byte> data_;
    size_t pos_{0};
};

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

int64_t midi_tick_to_beat_tick(uint64_t tick, uint16_t ppqn) {
    const long double scaled =
        static_cast<long double>(tick) *
        static_cast<long double>(time::BeatPosition::kTicksPerBeat) /
        static_cast<long double>(ppqn);
    return static_cast<int64_t>(std::llround(scaled));
}

struct ActiveNote {
    uint64_t start_tick{0};
    float velocity{0.8f};
};

struct NoteKey {
    uint8_t channel{0};
    uint8_t pitch{0};
    bool operator==(const NoteKey&) const noexcept = default;
};

struct NoteKeyHash {
    size_t operator()(const NoteKey& key) const noexcept {
        return (static_cast<size_t>(key.channel) << 8U) | key.pitch;
    }
};

} // namespace

std::optional<ImportedMidi> import_standard_midi(
    std::span<const std::byte> bytes, std::string* error) {
    Reader reader(bytes);
    const auto header_tag = reader.take(4);
    if (header_tag.size() != 4 ||
        std::to_integer<char>(header_tag[0]) != 'M' ||
        std::to_integer<char>(header_tag[1]) != 'T' ||
        std::to_integer<char>(header_tag[2]) != 'h' ||
        std::to_integer<char>(header_tag[3]) != 'd') {
        fail(error, "Missing MIDI header");
        return std::nullopt;
    }

    uint32_t header_length = 0;
    uint16_t format = 0;
    uint16_t track_count = 0;
    uint16_t division = 0;
    if (!reader.read_be32(header_length) || header_length < 6 ||
        !reader.read_be16(format) || !reader.read_be16(track_count) ||
        !reader.read_be16(division) || !reader.skip(header_length - 6)) {
        fail(error, "Truncated MIDI header");
        return std::nullopt;
    }
    if (format > 1 || track_count == 0) {
        fail(error, "Only MIDI format 0 and 1 are supported");
        return std::nullopt;
    }
    if ((division & 0x8000U) != 0 || division == 0) {
        fail(error, "SMPTE MIDI timing is not supported");
        return std::nullopt;
    }

    ImportedMidi result;
    int64_t max_end_ticks = 0;

    for (uint16_t track_index = 0; track_index < track_count; ++track_index) {
        const auto track_tag = reader.take(4);
        uint32_t track_length = 0;
        if (track_tag.size() != 4 ||
            std::to_integer<char>(track_tag[0]) != 'M' ||
            std::to_integer<char>(track_tag[1]) != 'T' ||
            std::to_integer<char>(track_tag[2]) != 'r' ||
            std::to_integer<char>(track_tag[3]) != 'k' ||
            !reader.read_be32(track_length) || reader.remaining() < track_length) {
            fail(error, "Invalid MIDI track chunk");
            return std::nullopt;
        }

        Reader track_reader(reader.take(track_length));
        ImportedMidiTrack imported_track;
        imported_track.name = "MIDI Track " + std::to_string(track_index + 1);
        uint64_t absolute_tick = 0;
        uint8_t running_status = 0;
        std::unordered_map<NoteKey, std::vector<ActiveNote>, NoteKeyHash> active;

        auto finish_note = [&](uint8_t channel, uint8_t pitch, float release_velocity) {
            const NoteKey key{channel, pitch};
            auto it = active.find(key);
            if (it == active.end() || it->second.empty()) return;
            const auto started = it->second.front();
            it->second.erase(it->second.begin());
            const auto start = midi_tick_to_beat_tick(started.start_tick, division);
            const auto end = midi_tick_to_beat_tick(absolute_tick, division);
            const auto duration = std::max<int64_t>(1, end - start);
            imported_track.notes.add_note(
                time::BeatPosition::from_ticks(start),
                time::BeatDuration::from_ticks(duration),
                static_cast<double>(pitch), started.velocity, release_velocity);
            max_end_ticks = std::max(max_end_ticks, start + duration);
        };

        while (track_reader.remaining() > 0) {
            uint32_t delta = 0;
            if (!track_reader.read_vlq(delta) ||
                absolute_tick > std::numeric_limits<uint64_t>::max() - delta) {
                fail(error, "Invalid MIDI delta time");
                return std::nullopt;
            }
            absolute_tick += delta;

            uint8_t first = 0;
            if (!track_reader.read_u8(first)) {
                fail(error, "Truncated MIDI event");
                return std::nullopt;
            }

            uint8_t status = first;
            bool has_first_data = false;
            uint8_t first_data = 0;
            if (first < 0x80U) {
                if (running_status < 0x80U || running_status >= 0xF0U) {
                    fail(error, "Invalid MIDI running status");
                    return std::nullopt;
                }
                status = running_status;
                has_first_data = true;
                first_data = first;
            } else if (first < 0xF0U) {
                running_status = first;
            }

            if (status == 0xFFU) {
                uint8_t type = 0;
                uint32_t length = 0;
                if (!track_reader.read_u8(type) || !track_reader.read_vlq(length)) {
                    fail(error, "Truncated MIDI meta event");
                    return std::nullopt;
                }
                const auto payload = track_reader.take(length);
                if (payload.size() != length) {
                    fail(error, "Truncated MIDI meta payload");
                    return std::nullopt;
                }
                if (type == 0x03U) {
                    imported_track.name.assign(
                        reinterpret_cast<const char*>(payload.data()), payload.size());
                } else if (type == 0x51U && length == 3 && !result.initial_bpm) {
                    const uint32_t micros =
                        (static_cast<uint32_t>(std::to_integer<uint8_t>(payload[0])) << 16U) |
                        (static_cast<uint32_t>(std::to_integer<uint8_t>(payload[1])) << 8U) |
                        static_cast<uint32_t>(std::to_integer<uint8_t>(payload[2]));
                    if (micros > 0) result.initial_bpm = 60000000.0 / micros;
                }
                running_status = 0;
                continue;
            }
            if (status == 0xF0U || status == 0xF7U) {
                uint32_t length = 0;
                if (!track_reader.read_vlq(length) || !track_reader.skip(length)) {
                    fail(error, "Truncated MIDI system event");
                    return std::nullopt;
                }
                running_status = 0;
                continue;
            }

            const uint8_t kind = status & 0xF0U;
            const uint8_t channel = status & 0x0FU;
            uint8_t data1 = first_data;
            uint8_t data2 = 0;
            if (!has_first_data && !track_reader.read_u8(data1)) {
                fail(error, "Truncated MIDI channel event");
                return std::nullopt;
            }
            const bool one_data_byte = kind == 0xC0U || kind == 0xD0U;
            if (!one_data_byte && !track_reader.read_u8(data2)) {
                fail(error, "Truncated MIDI channel event");
                return std::nullopt;
            }
            if (data1 > 127 || data2 > 127) {
                fail(error, "Invalid MIDI data byte");
                return std::nullopt;
            }

            if (kind == 0x90U && data2 != 0) {
                active[NoteKey{channel, data1}].push_back(
                    ActiveNote{absolute_tick, static_cast<float>(data2) / 127.0f});
            } else if (kind == 0x80U || (kind == 0x90U && data2 == 0)) {
                finish_note(channel, data1, static_cast<float>(data2) / 127.0f);
            }
        }

        for (auto& [key, starts] : active) {
            while (!starts.empty()) {
                finish_note(key.channel, key.pitch, 0.0f);
            }
        }
        if (!imported_track.notes.empty()) {
            result.tracks.push_back(std::move(imported_track));
        }
    }

    if (result.tracks.empty()) {
        fail(error, "MIDI file contains no note events");
        return std::nullopt;
    }
    result.length = time::BeatDuration::from_ticks(
        std::max<int64_t>(max_end_ticks, time::BeatPosition::kTicksPerBeat));
    if (error) error->clear();
    return result;
}

} // namespace saudade::model
