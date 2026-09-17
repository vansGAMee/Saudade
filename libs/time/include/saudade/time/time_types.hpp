#pragma once

#include <cstdint>
#include <compare>

namespace saudade::time {

/// Discrete sample position on the timeline (0-indexed).
using SamplePosition = int64_t;

/// Discrete sample duration/offset.
using SampleDuration = int64_t;

struct BeatPosition;

/// Exact musical duration represented as fixed-point ticks.
/// 960,000 ticks per quarter note (beat).
/// Exactly divides: 1/2, 1/3 (triplets), 1/4, 1/5 (quintuplets), 1/6, 1/8, 1/12, 1/16,
/// 1/24, 1/32, 1/48, 1/64, 1/128, etc. without floating-point drift.
struct BeatDuration {
    static constexpr int64_t kTicksPerBeat = 960000;

    int64_t ticks{0};

    constexpr BeatDuration() noexcept = default;
    constexpr explicit BeatDuration(int64_t t) noexcept : ticks(t) {}

    static constexpr BeatDuration from_ticks(int64_t t) noexcept {
        return BeatDuration{t};
    }

    static constexpr BeatDuration from_beats(int64_t whole_beats) noexcept {
        return BeatDuration{whole_beats * kTicksPerBeat};
    }

    static constexpr BeatDuration from_fraction(int64_t numerator, int64_t denominator) noexcept {
        return BeatDuration{(numerator * kTicksPerBeat) / denominator};
    }

    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(ticks) / static_cast<double>(kTicksPerBeat);
    }

    constexpr BeatDuration operator-() const noexcept {
        return BeatDuration{-ticks};
    }

    constexpr BeatDuration& operator+=(const BeatDuration& other) noexcept {
        ticks += other.ticks;
        return *this;
    }

    constexpr BeatDuration& operator-=(const BeatDuration& other) noexcept {
        ticks -= other.ticks;
        return *this;
    }

    friend constexpr BeatDuration operator+(BeatDuration lhs, const BeatDuration& rhs) noexcept {
        lhs += rhs;
        return lhs;
    }

    friend constexpr BeatDuration operator-(BeatDuration lhs, const BeatDuration& rhs) noexcept {
        lhs -= rhs;
        return lhs;
    }

    friend constexpr auto operator<=>(const BeatDuration& lhs, const BeatDuration& rhs) noexcept = default;
    friend constexpr bool operator==(const BeatDuration& lhs, const BeatDuration& rhs) noexcept = default;
};

/// Exact musical position on the timeline represented as fixed-point ticks.
struct BeatPosition {
    static constexpr int64_t kTicksPerBeat = BeatDuration::kTicksPerBeat;

    int64_t ticks{0};

    constexpr BeatPosition() noexcept = default;
    constexpr explicit BeatPosition(int64_t t) noexcept : ticks(t) {}

    static constexpr BeatPosition zero() noexcept {
        return BeatPosition{0};
    }

    static constexpr BeatPosition from_ticks(int64_t t) noexcept {
        return BeatPosition{t};
    }

    static constexpr BeatPosition from_beats(int64_t whole_beats) noexcept {
        return BeatPosition{whole_beats * kTicksPerBeat};
    }

    static constexpr BeatPosition from_fraction(int64_t numerator, int64_t denominator) noexcept {
        return BeatPosition{(numerator * kTicksPerBeat) / denominator};
    }

    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(ticks) / static_cast<double>(kTicksPerBeat);
    }

    constexpr BeatPosition& operator+=(const BeatDuration& dur) noexcept {
        ticks += dur.ticks;
        return *this;
    }

    constexpr BeatPosition& operator-=(const BeatDuration& dur) noexcept {
        ticks -= dur.ticks;
        return *this;
    }

    friend constexpr BeatPosition operator+(BeatPosition pos, const BeatDuration& dur) noexcept {
        pos += dur;
        return pos;
    }

    friend constexpr BeatPosition operator+(const BeatDuration& dur, BeatPosition pos) noexcept {
        pos += dur;
        return pos;
    }

    friend constexpr BeatPosition operator-(BeatPosition pos, const BeatDuration& dur) noexcept {
        pos -= dur;
        return pos;
    }

    friend constexpr BeatDuration operator-(const BeatPosition& lhs, const BeatPosition& rhs) noexcept {
        return BeatDuration{lhs.ticks - rhs.ticks};
    }

    friend constexpr auto operator<=>(const BeatPosition& lhs, const BeatPosition& rhs) noexcept = default;
    friend constexpr bool operator==(const BeatPosition& lhs, const BeatPosition& rhs) noexcept = default;
};

} // namespace saudade::time
