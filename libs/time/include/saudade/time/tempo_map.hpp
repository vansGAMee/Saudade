#pragma once

#include <saudade/time/time_types.hpp>

namespace saudade::time {

/// Represents the timeline tempo structure and provides sample <-> beat conversions.
/// Designed to support multiple segments and curves in the future;
/// currently implements a single constant tempo segment at 120 BPM default.
class TempoMap {
public:
    static constexpr double kDefaultBpm = 120.0;

    explicit TempoMap(double bpm = kDefaultBpm) noexcept;

    [[nodiscard]] double bpm() const noexcept { return bpm_; }

    /// Converts a musical BeatPosition to a timeline SamplePosition for a given sample rate.
    [[nodiscard]] SamplePosition beat_to_sample(BeatPosition beat, double sample_rate) const noexcept;

    /// Converts a timeline SamplePosition to a musical BeatPosition for a given sample rate.
    [[nodiscard]] BeatPosition sample_to_beat(SamplePosition sample, double sample_rate) const noexcept;

private:
    double bpm_{kDefaultBpm};
};

} // namespace saudade::time
