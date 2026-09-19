#pragma once

#include <saudade/time/time_types.hpp>
#include <saudade/time/tempo_map.hpp>

#include <atomic>
#include <cstdint>

namespace saudade::time {

/// High-level transport states.
enum class TransportState : uint8_t {
    Stopped = 0,
    Playing = 1
};

/// Immutable realtime snapshot captured once at the start of each audio quantum.
struct TransportSnapshot {
    bool playing{false};
    SamplePosition block_start_sample{0};
    bool loop_enabled{false};
    SamplePosition loop_start_sample{0};
    SamplePosition loop_end_sample{0};
};

/// Transport controller managing the timeline playback state and position.
///
/// Control thread:
/// - Commands: play(), stop(), seek_samples(), seek_beats(), set_loop_*(), set_bpm()
/// - Queries: state(), is_playing(), current_sample(), current_beat(), is_loop_enabled(), etc.
///
/// Realtime audio thread:
/// - acquire_snapshot(): Captured once at the beginning of process() [lock-free, noexcept]
/// - advance_quantum(): Advances position by num_frames if playing, wraps if looping [lock-free, noexcept]
class TransportController {
public:
    explicit TransportController(TempoMap tempo_map = TempoMap{}) noexcept;
    ~TransportController() = default;

    TransportController(const TransportController&) = delete;
    TransportController& operator=(const TransportController&) = delete;
    TransportController(TransportController&&) = delete;
    TransportController& operator=(TransportController&&) = delete;

    // --- Control-thread API ---

    /// Starts playback from current position.
    void play() noexcept;

    /// Stops playback, freezing the timeline position.
    void stop() noexcept;

    /// Seeks to an exact discrete sample position on the timeline.
    void seek_samples(SamplePosition sample_pos) noexcept;

    /// Seeks to an exact musical BeatPosition using the TempoMap and actual sample rate.
    void seek_beats(BeatPosition beat_pos, double sample_rate) noexcept;

    /// Controls loop playback.
    void set_loop_enabled(bool enabled) noexcept;
    void set_loop_range_samples(SamplePosition start_sample, SamplePosition end_sample) noexcept;
    void set_loop_range_beats(BeatPosition start_beat, BeatPosition end_beat, double sample_rate) noexcept;

    [[nodiscard]] bool is_loop_enabled() const noexcept;
    [[nodiscard]] SamplePosition loop_start_sample() const noexcept;
    [[nodiscard]] SamplePosition loop_end_sample() const noexcept;

    [[nodiscard]] TransportState state() const noexcept;
    [[nodiscard]] bool is_playing() const noexcept;
    [[nodiscard]] SamplePosition current_sample() const noexcept;
    [[nodiscard]] BeatPosition current_beat(double sample_rate) const noexcept;

    [[nodiscard]] const TempoMap& tempo_map() const noexcept { return tempo_map_; }
    void set_tempo_map(TempoMap map) noexcept { tempo_map_ = map; }
    void set_bpm(double bpm) noexcept { tempo_map_.set_bpm(bpm); }

    // --- Realtime-thread API (Hard Realtime Safe: lock-free, zero-alloc, noexcept) ---

    /// Captures the transport state snapshot for the current audio quantum.
    /// Must be called once per audio block.
    [[nodiscard]] TransportSnapshot acquire_snapshot() noexcept;

    /// Advances the timeline position by the processed frame count if playing.
    /// Wraps around if loop is enabled and loop boundary is crossed.
    void advance_quantum(uint32_t num_frames) noexcept;

private:
    static_assert(std::atomic<bool>::is_always_lock_free,
                  "std::atomic<bool> must be lock-free on this platform");
    static_assert(std::atomic<SamplePosition>::is_always_lock_free,
                  "std::atomic<SamplePosition> must be lock-free on this platform");
    static_assert(std::atomic<uint64_t>::is_always_lock_free,
                  "std::atomic<uint64_t> must be lock-free on this platform");

    TempoMap tempo_map_;

    // Control -> RT atomics
    std::atomic<bool> playing_requested_{false};
    std::atomic<SamplePosition> seek_target_{0};
    std::atomic<uint64_t> seek_version_{0};
    std::atomic<bool> loop_enabled_requested_{false};
    std::atomic<SamplePosition> loop_start_requested_{0};
    std::atomic<SamplePosition> loop_end_requested_{0};

    // RT -> Control atomics
    std::atomic<uint64_t> acknowledged_seek_version_{0};
    std::atomic<SamplePosition> rt_position_{0};
    std::atomic<bool> rt_playing_{false};

    // Local RT state (only accessed by single realtime thread)
    SamplePosition rt_current_sample_{0};
    uint64_t last_seen_seek_version_{0};
    TransportSnapshot active_snapshot_{false, 0, false, 0, 0};
    uint64_t snapshotted_seek_version_{0};
};

} // namespace saudade::time
