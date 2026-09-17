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

    // 2. Snapshot current play state
    const bool playing = playing_requested_.load(std::memory_order_acquire);
    rt_playing_.store(playing, std::memory_order_release);

    active_snapshot_ = TransportSnapshot{
        .playing = playing,
        .block_start_sample = rt_current_sample_
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
            rt_position_.store(rt_current_sample_, std::memory_order_release);
        }
    }
}

} // namespace saudade::time
