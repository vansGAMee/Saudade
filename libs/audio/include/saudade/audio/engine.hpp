#pragma once

#include <saudade/audio/audio_block.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/process_context.hpp>
#include <saudade/audio/plan_publisher.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/time/transport.hpp>

#include <memory>
#include <cstdint>

namespace saudade::audio {

/// Audio engine executing precompiled, immutable RenderPlans.
/// Supports live atomic replacement of the active plan at quantum boundaries
/// and timeline transport control with zero allocations and zero locks on the realtime path.
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
    void seek_samples(time::SamplePosition pos) noexcept { transport_.seek_samples(pos); }
    void seek_beats(time::BeatPosition beat, double sample_rate) noexcept {
        transport_.seek_beats(beat, sample_rate);
    }

    /// Returns the currently active RenderPlan.
    [[nodiscard]] const renderplan::RenderPlan& plan() const { return publisher_.active_plan(); }

    /// Accesses the underlying PlanPublisher.
    [[nodiscard]] PlanPublisher& publisher() noexcept { return publisher_; }
    [[nodiscard]] const PlanPublisher& publisher() const noexcept { return publisher_; }

    /// Accesses the TransportController.
    [[nodiscard]] time::TransportController& transport() noexcept { return transport_; }
    [[nodiscard]] const time::TransportController& transport() const noexcept { return transport_; }

    [[nodiscard]] uint32_t max_block_size() const noexcept { return max_block_size_; }

private:
    uint32_t max_block_size_{kDefaultMaxBlockSize};
    PlanPublisher publisher_;
    time::TransportController transport_;
};

} // namespace saudade::audio
