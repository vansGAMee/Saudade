#pragma once

#include <saudade/audio/audio_block.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/process_context.hpp>
#include <saudade/events/event_block.hpp>
#include <saudade/renderplan/dsp_state.hpp>
#include <saudade/renderplan/execution_step.hpp>

#include <vector>
#include <cstdint>
#include <cstddef>

namespace saudade::renderplan {

/// Structurally immutable execution plan prepared by GraphCompiler.
/// The realtime audio thread only executes this prepared plan.
class RenderPlan {
public:
    RenderPlan(std::vector<ExecutionStep> steps,
               uint32_t num_scratch_buffers,
               size_t num_sine_states,
               size_t output_channels,
               size_t num_synth_states = 0);

    virtual ~RenderPlan() = default;

    RenderPlan(const RenderPlan&) = default;
    RenderPlan& operator=(const RenderPlan&) = default;
    RenderPlan(RenderPlan&&) noexcept = default;
    RenderPlan& operator=(RenderPlan&&) noexcept = default;

    [[nodiscard]] const std::vector<ExecutionStep>& steps() const noexcept { return steps_; }
    [[nodiscard]] uint32_t num_scratch_buffers() const noexcept { return num_scratch_buffers_; }
    [[nodiscard]] size_t num_sine_states() const noexcept { return num_sine_states_; }
    [[nodiscard]] size_t num_synth_states() const noexcept { return num_synth_states_; }
    [[nodiscard]] size_t output_channels() const noexcept { return output_channels_; }

    /// Creates and preallocates the mutable DSP state container required by this plan.
    [[nodiscard]] DspStateStorage create_dsp_state() const;

    /// Realtime execution path. Hard realtime safe: zero allocations, zero locks.
    void render(audio::AudioBlock& output_block,
                audio::AudioBuffer& scratch_buffers,
                DspStateStorage& dsp_state,
                const audio::ProcessContext& ctx,
                events::EventBlockView events = {}) const noexcept;

private:
    std::vector<ExecutionStep> steps_;
    uint32_t num_scratch_buffers_{0};
    size_t num_sine_states_{0};
    size_t num_synth_states_{0};
    size_t output_channels_{0};
};

} // namespace saudade::renderplan
