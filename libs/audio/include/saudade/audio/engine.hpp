#pragma once

#include <saudade/audio/audio_block.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/process_context.hpp>
#include <saudade/audio/plan_publisher.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/time/transport.hpp>
#include <saudade/events/event_queue.hpp>
#include <saudade/events/event_block.hpp>

#include <memory>
#include <cstdint>
#include <atomic>
#include <chrono>

namespace saudade::audio {

/// Audio engine executing precompiled, immutable RenderPlans.
/// Supports live atomic replacement of the active plan at quantum boundaries,
/// timeline transport control, and sample-accurate event ingress
/// with zero allocations and zero locks on the realtime path.
class AudioEngine {
public:
    static constexpr uint32_t kDefaultMaxBlockSize = 8192;

    explicit AudioEngine(std::unique_ptr<const renderplan::RenderPlan> initial_plan,
                         uint32_t max_block_size = kDefaultMaxBlockSize);

    ~AudioEngine() = default;

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    /// Prepares buffers for a maximum block size outside the realtime path.
    void prepare(uint32_t max_block_size);

    /// Resets runtime DSP state (e.g. oscillator phase). Outside realtime path.
    void reset();

    /// Realtime render path. Hard realtime safe: zero allocations, zero locks.
    /// Snapshots active plan and transport state once at quantum start.
    void process(AudioBlock& output_block, const ProcessContext& ctx) noexcept;

    /// Publishes a new RenderPlan generation with unique ownership from the control thread.
    PlanGeneration publish_plan(std::unique_ptr<const renderplan::RenderPlan> new_plan);

    /// Collects and destroys retired plans outside the realtime path.
    size_t collect_retired();

    /// Control-thread transport operations.
    void play() noexcept { transport_.play(); }
    void stop() noexcept { transport_.stop(); }
    void seek_samples(time::SamplePosition pos) noexcept {
        transport_.seek_samples(pos);
        event_queue_.reset_schedule_tracking(pos);
    }
    void seek_beats(time::BeatPosition beat, double sample_rate) noexcept {
        const auto sample_pos = transport_.tempo_map().beat_to_sample(beat, sample_rate);
        seek_samples(sample_pos);
    }

    /// Control-thread event scheduling API.
    bool schedule_note_on(time::SamplePosition pos, events::NoteId id, double pitch, float velocity) noexcept {
        return event_queue_.schedule_note_on(pos, id, pitch, velocity);
    }

    bool schedule_note_off(time::SamplePosition pos, events::NoteId id, float release_velocity = 0.0f) noexcept {
        return event_queue_.schedule_note_off(pos, id, release_velocity);
    }

    bool schedule_event(const events::TimelineEvent& event) noexcept {
        return event_queue_.schedule_event(event);
    }

    /// Returns the currently active RenderPlan.
    [[nodiscard]] const renderplan::RenderPlan& plan() const { return publisher_.active_plan(); }

    /// Accesses the underlying PlanPublisher.
    [[nodiscard]] PlanPublisher& publisher() noexcept { return publisher_; }
    [[nodiscard]] const PlanPublisher& publisher() const noexcept { return publisher_; }

    /// Accesses the TransportController.
    [[nodiscard]] time::TransportController& transport() noexcept { return transport_; }
    [[nodiscard]] const time::TransportController& transport() const noexcept { return transport_; }

    /// Accesses the EventQueue.
    [[nodiscard]] events::EventQueue& event_queue() noexcept { return event_queue_; }
    [[nodiscard]] const events::EventQueue& event_queue() const noexcept { return event_queue_; }

    [[nodiscard]] uint32_t max_block_size() const noexcept { return max_block_size_; }

    /// Requests an asynchronous RT-safe event flush.
    /// Realtime thread will drain the event queue and reset active synth DSP voices
    /// at the start of the next audio quantum without taking any locks or reallocating memory.
    uint64_t request_event_flush() noexcept;

    /// Checks if a flush generation has been acknowledged by the realtime thread.
    [[nodiscard]] bool is_flush_acknowledged(uint64_t generation) const noexcept;

    /// Blocks (yielding CPU) on the control thread until the realtime thread acknowledges the flush.
    bool wait_for_flush(uint64_t generation,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) noexcept;

    /// Synchronous helper for control thread: requests flush and waits for acknowledgment.
    /// Resets control-side schedule tracking to 0 after acknowledgment.
    bool flush_events(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) noexcept;

private:
    uint32_t max_block_size_{kDefaultMaxBlockSize};
    PlanPublisher publisher_;
    time::TransportController transport_;
    events::EventQueue event_queue_;
    events::EventBlock quantum_event_block_;

    alignas(64) std::atomic<uint64_t> flush_requested_{0};
    alignas(64) std::atomic<uint64_t> flush_acknowledged_{0};
    uint64_t next_flush_request_{0};
};

} // namespace saudade::audio
