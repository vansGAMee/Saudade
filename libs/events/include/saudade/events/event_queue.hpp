#pragma once

#include <saudade/events/event_types.hpp>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

namespace saudade::events {

/// Single-Producer Single-Consumer (SPSC) lock-free event ingress queue.
/// Transfers TimelineEvents from the non-realtime control thread to the single realtime audio thread.
///
/// Invariants:
/// - Fixed capacity power-of-two ring buffer.
/// - Control-side events must be scheduled in non-decreasing absolute SamplePosition order.
/// - Out-of-order events are strictly rejected.
/// - Buffer overflow does not reallocate; events are rejected and counted.
/// - Realtime consumer is lock-free, wait-free, zero-allocation, and non-blocking.
template <size_t Capacity = 2048>
class SpscEventQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(std::atomic<size_t>::is_always_lock_free, "std::atomic<size_t> must be lock-free");

    static constexpr size_t kCapacity = Capacity;
    static constexpr size_t kMask = Capacity - 1;

    SpscEventQueue() noexcept = default;

    // --- Control-Producer API ---

    /// Schedules a NoteOn event at an absolute timeline SamplePosition.
    bool schedule_note_on(time::SamplePosition pos, NoteId id, double pitch, float velocity) noexcept {
        return schedule_event(TimelineEvent(pos, EventPayload(NoteOn{id, pitch, velocity})));
    }

    /// Schedules a NoteOff event at an absolute timeline SamplePosition.
    bool schedule_note_off(time::SamplePosition pos, NoteId id, float release_velocity = 0.0f) noexcept {
        return schedule_event(TimelineEvent(pos, EventPayload(NoteOff{id, release_velocity})));
    }

    /// Schedules a generic TimelineEvent.
    bool schedule_event(const TimelineEvent& event) noexcept {
        // Enforce non-decreasing absolute sample order on control side
        if (has_scheduled_events_ && event.sample_position < last_scheduled_sample_) {
            ++rejected_out_of_order_count_;
            return false;
        }

        const size_t w = write_index_.load(std::memory_order_relaxed);
        const size_t r = read_index_.load(std::memory_order_acquire);

        if (w - r >= kCapacity) {
            ++dropped_overflow_count_;
            return false;
        }

        buffer_[w & kMask] = event;
        write_index_.store(w + 1, std::memory_order_release);

        last_scheduled_sample_ = event.sample_position;
        has_scheduled_events_ = true;
        return true;
    }

    /// Resets the monotonic schedule tracking, e.g. after a transport seek backwards.
    void reset_schedule_tracking(time::SamplePosition new_min_pos = 0) noexcept {
        last_scheduled_sample_ = new_min_pos;
        has_scheduled_events_ = false;
    }

    [[nodiscard]] uint64_t dropped_overflow_count() const noexcept { return dropped_overflow_count_; }
    [[nodiscard]] uint64_t rejected_out_of_order_count() const noexcept { return rejected_out_of_order_count_; }

    // --- Realtime-Consumer API (Lock-Free, Wait-Free, Zero-Alloc) ---

    /// Peeks at the next available event without removing it.
    [[nodiscard]] bool peek(TimelineEvent& out) const noexcept {
        const size_t r = read_index_.load(std::memory_order_relaxed);
        const size_t w = write_index_.load(std::memory_order_acquire);

        if (r == w) {
            return false;
        }

        out = buffer_[r & kMask];
        return true;
    }

    /// Discards the front event after processing.
    bool pop() noexcept {
        const size_t r = read_index_.load(std::memory_order_relaxed);
        const size_t w = write_index_.load(std::memory_order_acquire);

        if (r == w) {
            return false;
        }

        read_index_.store(r + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const size_t r = read_index_.load(std::memory_order_relaxed);
        const size_t w = write_index_.load(std::memory_order_acquire);
        return r == w;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t r = read_index_.load(std::memory_order_relaxed);
        const size_t w = write_index_.load(std::memory_order_acquire);
        return w >= r ? (w - r) : 0;
    }

    void clear() noexcept {
        const size_t w = write_index_.load(std::memory_order_relaxed);
        read_index_.store(w, std::memory_order_release);
    }

private:
    std::array<TimelineEvent, kCapacity> buffer_{};

    // Cache-aligned indices to prevent false sharing between control and RT threads
    alignas(64) std::atomic<size_t> write_index_{0};
    alignas(64) std::atomic<size_t> read_index_{0};

    // Control-only state
    time::SamplePosition last_scheduled_sample_{0};
    bool has_scheduled_events_{false};
    uint64_t dropped_overflow_count_{0};
    uint64_t rejected_out_of_order_count_{0};
};

using EventQueue = SpscEventQueue<2048>;

} // namespace saudade::events
