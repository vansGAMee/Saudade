#include <saudade/time/tempo_map.hpp>
#include <cmath>

namespace saudade::time {

TempoMap::TempoMap(double bpm) noexcept
    : bpm_(bpm > 0.0 ? bpm : kDefaultBpm) {}

SamplePosition TempoMap::beat_to_sample(BeatPosition beat, double sample_rate) const noexcept {
    if (sample_rate <= 0.0 || bpm_ <= 0.0) {
        return 0;
    }
    const double beats = static_cast<double>(beat.ticks) / static_cast<double>(BeatPosition::kTicksPerBeat);
    const double seconds = beats * (60.0 / bpm_);
    return static_cast<SamplePosition>(std::llround(seconds * sample_rate));
}

BeatPosition TempoMap::sample_to_beat(SamplePosition sample, double sample_rate) const noexcept {
    if (sample_rate <= 0.0 || bpm_ <= 0.0) {
        return BeatPosition::zero();
    }
    const double seconds = static_cast<double>(sample) / sample_rate;
    const double beats = seconds * (bpm_ / 60.0);
    const int64_t ticks = static_cast<int64_t>(std::llround(beats * static_cast<double>(BeatPosition::kTicksPerBeat)));
    return BeatPosition::from_ticks(ticks);
}

} // namespace saudade::time
