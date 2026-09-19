#include <saudade/time/transport.hpp>

namespace saudade::time {

TransportController::TransportController(TempoMap tempo_map) noexcept
    : tempo_map_(tempo_map) {}

void TransportController::play() noexcept {
    playing_requested_.store(true, std::memory_order_release);
}

void TransportController::stop() noexcept {
    playing_requested_.store(false, std::memory_order_release);
}

void TransportController::seek_samples(SamplePosition sample_pos) noexcept {
    if (sample_pos < 0) {
        sample_pos = 0;
    }
    seek_target_.store(sample_pos, std::memory_order_release);
    seek_version_.fetch_add(1, std::memory_order_release);
}

void TransportController::seek_beats(BeatPosition beat_pos, double sample_rate) noexcept {
    seek_samples(tempo_map_.beat_to_sample(beat_pos, sample_rate));
}

void TransportController::set_loop_enabled(bool enabled) noexcept {
    loop_enabled_requested_.store(enabled, std::memory_order_release);
}

void TransportController::set_loop_range_samples(SamplePosition start_sample, SamplePosition end_sample) noexcept {
    if (start_sample < 0) {
        start_sample = 0;
    }
    if (end_sample < start_sample) {
        end_sample = start_sample;
    }
    loop_start_requested_.store(start_sample, std::memory_order_release);
    loop_end_requested_.store(end_sample, std::memory_order_release);
}

void TransportController::set_loop_range_beats(BeatPosition start_beat, BeatPosition end_beat, double sample_rate) noexcept {
    const auto s = tempo_map_.beat_to_sample(start_beat, sample_rate);
    const auto e = tempo_map_.beat_to_sample(end_beat, sample_rate);
    set_loop_range_samples(s, e);
}

bool TransportController::is_loop_enabled() const noexcept {
    return loop_enabled_requested_.load(std::memory_order_acquire);
}

SamplePosition TransportController::loop_start_sample() const noexcept {
    return loop_start_requested_.load(std::memory_order_acquire);
}

SamplePosition TransportController::loop_end_sample() const noexcept {
    return loop_end_requested_.load(std::memory_order_acquire);
}

TransportState TransportController::state() const noexcept {
    return is_playing() ? TransportState::Playing : TransportState::Stopped;
}

bool TransportController::is_playing() const noexcept {
    return playing_requested_.load(std::memory_order_acquire);
}

SamplePosition TransportController::current_sample() const noexcept {
    const uint64_t target_v = seek_version_.load(std::memory_order_acquire);
    const uint64_t ack_v = acknowledged_seek_version_.load(std::memory_order_acquire);
    if (target_v != ack_v) {
        return seek_target_.load(std::memory_order_relaxed);
    }
    return rt_position_.load(std::memory_order_acquire);
}

BeatPosition TransportController::current_beat(double sample_rate) const noexcept {
    return tempo_map_.sample_to_beat(current_sample(), sample_rate);
}

TransportSnapshot TransportController::acquire_snapshot() noexcept {
    // 1. Process any pending seek commands
    const uint64_t v = seek_version_.load(std::memory_order_acquire);
    if (v != last_seen_seek_version_) {
        last_seen_seek_version_ = v;
        rt_current_sample_ = seek_target_.load(std::memory_order_relaxed);
        rt_position_.store(rt_current_sample_, std::memory_order_release);
        acknowledged_seek_version_.store(v, std::memory_order_release);
    }

    // 2. Snapshot current play and loop state
    const bool playing = playing_requested_.load(std::memory_order_acquire);
    rt_playing_.store(playing, std::memory_order_release);

    const bool loop_enabled = loop_enabled_requested_.load(std::memory_order_acquire);
    const SamplePosition loop_start = loop_start_requested_.load(std::memory_order_acquire);
    const SamplePosition loop_end = loop_end_requested_.load(std::memory_order_acquire);

    active_snapshot_ = TransportSnapshot{
        .playing = playing,
        .block_start_sample = rt_current_sample_,
        .loop_enabled = loop_enabled,
        .loop_start_sample = loop_start,
        .loop_end_sample = loop_end
    };
    snapshotted_seek_version_ = last_seen_seek_version_;
    return active_snapshot_;
}

void TransportController::advance_quantum(uint32_t num_frames) noexcept {
    if (active_snapshot_.playing && num_frames > 0) {
        // Only advance position if no new seek arrived during the quantum execution
        const uint64_t v = seek_version_.load(std::memory_order_acquire);
        if (v == snapshotted_seek_version_) {
            rt_current_sample_ += static_cast<SamplePosition>(num_frames);
            if (active_snapshot_.loop_enabled &&
                active_snapshot_.loop_end_sample > active_snapshot_.loop_start_sample) {
                const auto loop_len = active_snapshot_.loop_end_sample - active_snapshot_.loop_start_sample;
                if (rt_current_sample_ >= active_snapshot_.loop_end_sample) {
                    const auto excess = rt_current_sample_ - active_snapshot_.loop_end_sample;
                    rt_current_sample_ = active_snapshot_.loop_start_sample + (excess % loop_len);
                }
            }
            rt_position_.store(rt_current_sample_, std::memory_order_release);
        }
    }
}

} // namespace saudade::time
