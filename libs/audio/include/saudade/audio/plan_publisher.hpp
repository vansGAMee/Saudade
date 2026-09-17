#pragma once

#include <saudade/audio/audio_buffer.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/renderplan/dsp_state.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace saudade::audio {

using PlanGeneration = uint64_t;

/// An execution package preallocated for a specific RenderPlan generation.
/// Prepared entirely on the control thread before publication to the realtime thread.
/// Uniquely owns the RenderPlan and its runtime execution resources.
struct PreparedPlan {
    PlanGeneration generation{0};
    std::unique_ptr<const renderplan::RenderPlan> plan;
    renderplan::DspStateStorage dsp_state;
    audio::AudioBuffer scratch_buffers;

    // Minimum completed generation required before this plan can be safely reclaimed.
    PlanGeneration retire_barrier_generation{0};
};

/// Lock-free exchange and lifetime manager for immutable RenderPlans.
/// Control side uniquely owns the plans; realtime thread observes via raw pointer.
///
/// Publication protocol (Control thread):
/// 1. Preallocates and configures PreparedPlan completely with unique plan ownership.
/// 2. Sets retire_barrier_generation on current active plan to the new generation.
/// 3. Moves old plan to retired list.
/// 4. Release-stores pointer to new plan into active_plan_.
///
/// Observation protocol (Realtime audio thread):
/// 1. Acquire-loads active_plan_ exactly once at the beginning of the audio quantum.
/// 2. Executes the quantum using the snapshotted PreparedPlan.
/// 3. Release-stores the executed generation into completed_generation_.
///
/// Reclamation protocol (Control thread):
/// 1. Acquire-loads completed_generation_.
/// 2. Erases any retired plan whose retire_barrier_generation <= completed_generation_.
class PlanPublisher {
public:
    PlanPublisher();
    ~PlanPublisher();

    PlanPublisher(const PlanPublisher&) = delete;
    PlanPublisher& operator=(const PlanPublisher&) = delete;
    PlanPublisher(PlanPublisher&&) = delete;
    PlanPublisher& operator=(PlanPublisher&&) = delete;

    // --- Control-thread API ---

    /// Prepares and atomically publishes a new RenderPlan generation with unique ownership.
    /// Allocates DSP state and scratch buffers prior to publication.
    PlanGeneration publish(std::unique_ptr<const renderplan::RenderPlan> plan, uint32_t max_block_size);

    /// Prepares buffers of the active plan for a new maximum block size. Outside realtime path.
    void prepare(uint32_t max_block_size);

    /// Collects and destroys retired plans that are no longer referenced by the realtime thread.
    /// Returns the number of plans reclaimed.
    size_t collect_retired();

    /// Returns the number of currently retired plans waiting for reclamation.
    [[nodiscard]] size_t retired_count() const;

    /// Returns the generation of the currently active plan, or 0 if none.
    [[nodiscard]] PlanGeneration active_generation() const;

    /// Returns the highest generation completed by the realtime thread.
    [[nodiscard]] PlanGeneration completed_generation() const noexcept;

    /// Checks if a valid active plan is published.
    [[nodiscard]] bool has_active_plan() const noexcept;

    /// Accesses the active RenderPlan on the control side.
    [[nodiscard]] const renderplan::RenderPlan& active_plan() const;

    /// Resets the DSP runtime state of the active plan. Outside realtime path.
    void reset_active_dsp_state();

    // --- Realtime-thread API (Hard Realtime Safe: lock-free, zero-alloc, noexcept) ---

    /// Snapshots the active plan for the current audio quantum.
    /// Must be called exactly once per quantum.
    [[nodiscard]] PreparedPlan* acquire_current_for_quantum() noexcept;

    /// Acknowledges completion of the audio quantum for the specified generation.
    void acknowledge_completed_generation(PlanGeneration gen) noexcept;

private:
    static_assert(std::atomic<PreparedPlan*>::is_always_lock_free,
                  "std::atomic<PreparedPlan*> must be lock-free on this platform");
    static_assert(std::atomic<PlanGeneration>::is_always_lock_free,
                  "std::atomic<PlanGeneration> must be lock-free on this platform");

    // Atomic exchange primitives accessed by realtime thread
    std::atomic<PreparedPlan*> active_plan_{nullptr};
    std::atomic<PlanGeneration> completed_generation_{0};

    // Control-thread ownership and synchronization
    mutable std::mutex control_mutex_;
    std::unique_ptr<PreparedPlan> active_holder_;
    std::vector<std::unique_ptr<PreparedPlan>> retired_plans_;
    PlanGeneration next_generation_{0};
};

} // namespace saudade::audio
