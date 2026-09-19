#pragma once

#include <saudade/time/time_types.hpp>

#include <cstdint>
#include <variant>
#include <type_traits>

namespace saudade::events {

/// Monotonic identifier for a discrete note lifecycle.
using NoteId = uint64_t;

/// NoteOn event payload.
/// Pitch is fractional semitones: 69.0 = A4 (440 Hz), 60.0 = C4.
/// Velocity is normalized [0.0, 1.0].
struct NoteOn {
    NoteId note_id{0};
    double pitch{69.0};
    float velocity{1.0f};
    float gain{1.0f};
    float pan{0.0f};

    constexpr bool operator==(const NoteOn&) const noexcept = default;
};

/// NoteOff event payload.
/// Release velocity is normalized [0.0, 1.0].
struct NoteOff {
    NoteId note_id{0};
    float release_velocity{0.0f};

    constexpr bool operator==(const NoteOff&) const noexcept = default;
};

/// Discriminated union of core musical events.
using EventPayload = std::variant<NoteOn, NoteOff>;

/// Event scheduled for a specific discrete sample offset within the active quantum.
/// Invariant: 0 <= sample_offset < num_frames.
struct TimedEvent {
    uint32_t sample_offset{0};
    EventPayload payload{};

    constexpr TimedEvent() noexcept = default;
    constexpr TimedEvent(uint32_t offset, EventPayload p) noexcept
        : sample_offset(offset), payload(p) {}

    constexpr bool operator==(const TimedEvent&) const noexcept = default;
};

static_assert(std::is_trivially_copyable_v<TimedEvent>,
              "TimedEvent must be trivially copyable for real-time safety");

/// Absolute timeline event submitted by the control thread to the ingress queue.
struct TimelineEvent {
    time::SamplePosition sample_position{0};
    EventPayload payload{};

    constexpr TimelineEvent() noexcept = default;
    constexpr TimelineEvent(time::SamplePosition pos, EventPayload p) noexcept
        : sample_position(pos), payload(p) {}

    constexpr bool operator==(const TimelineEvent&) const noexcept = default;
};

static_assert(std::is_trivially_copyable_v<TimelineEvent>,
              "TimelineEvent must be trivially copyable for real-time safety");

/// Deterministic ordering for TimedEvents within a quantum:
/// 1. Ascending by sample_offset
/// 2. NoteOff before NoteOn on identical sample_offset
/// 3. Ascending by NoteId
inline bool operator<(const TimedEvent& lhs, const TimedEvent& rhs) noexcept {
    if (lhs.sample_offset != rhs.sample_offset) {
        return lhs.sample_offset < rhs.sample_offset;
    }
    const auto get_rank = [](const EventPayload& p) noexcept -> int {
        return std::holds_alternative<NoteOff>(p) ? 0 : 1;
    };
    const int rank_l = get_rank(lhs.payload);
    const int rank_r = get_rank(rhs.payload);
    if (rank_l != rank_r) {
        return rank_l < rank_r;
    }
    const auto get_id = [](const EventPayload& p) noexcept -> NoteId {
        if (std::holds_alternative<NoteOff>(p)) return std::get<NoteOff>(p).note_id;
        return std::get<NoteOn>(p).note_id;
    };
    return get_id(lhs.payload) < get_id(rhs.payload);
}

/// Deterministic ordering for TimelineEvents:
/// 1. Ascending by sample_position
/// 2. NoteOff before NoteOn on identical sample_position
/// 3. Ascending by NoteId
inline bool operator<(const TimelineEvent& lhs, const TimelineEvent& rhs) noexcept {
    if (lhs.sample_position != rhs.sample_position) {
        return lhs.sample_position < rhs.sample_position;
    }
    const auto get_rank = [](const EventPayload& p) noexcept -> int {
        return std::holds_alternative<NoteOff>(p) ? 0 : 1;
    };
    const int rank_l = get_rank(lhs.payload);
    const int rank_r = get_rank(rhs.payload);
    if (rank_l != rank_r) {
        return rank_l < rank_r;
    }
    const auto get_id = [](const EventPayload& p) noexcept -> NoteId {
        if (std::holds_alternative<NoteOff>(p)) return std::get<NoteOff>(p).note_id;
        return std::get<NoteOn>(p).note_id;
    };
    return get_id(lhs.payload) < get_id(rhs.payload);
}

} // namespace saudade::events
