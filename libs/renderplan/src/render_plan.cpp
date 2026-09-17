#include <saudade/renderplan/render_plan.hpp>
#include <cmath>
#include <cstring>
#include <numbers>

namespace saudade::renderplan {

RenderPlan::RenderPlan(std::vector<ExecutionStep> steps,
                       uint32_t num_scratch_buffers,
                       size_t num_sine_states,
                       size_t output_channels)
    : steps_(std::move(steps)),
      num_scratch_buffers_(num_scratch_buffers),
      num_sine_states_(num_sine_states),
      output_channels_(output_channels) {}

DspStateStorage RenderPlan::create_dsp_state() const {
    DspStateStorage storage;
    storage.allocate_sine_states(num_sine_states_);
    return storage;
}

void RenderPlan::render(audio::AudioBlock& output_block,
                        audio::AudioBuffer& scratch_buffers,
                        DspStateStorage& dsp_state,
                        const audio::ProcessContext& ctx) const noexcept {
    const uint32_t n_frames = ctx.num_frames;
    if (n_frames == 0) {
        return;
    }

    constexpr float two_pi = static_cast<float>(2.0 * std::numbers::pi_v<double>);
    const double sr = ctx.sample_rate > 0.0 ? ctx.sample_rate : 48000.0;

    for (const auto& step : steps_) {
        std::visit([&](const auto& s) noexcept {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, SineStep>) {
                float* out = scratch_buffers.channel(s.output_buffer_slot);
                SineState& state = dsp_state.sine_state(s.state_index);
                const float phase_step = static_cast<float>(
                    (2.0 * std::numbers::pi_v<double> * static_cast<double>(s.frequency)) / sr
                );
                float phase = state.phase;

                for (uint32_t i = 0; i < n_frames; ++i) {
                    out[i] = std::sin(phase);
                    phase += phase_step;
                    if (phase >= two_pi) {
                        phase -= two_pi;
                    }
                }
                state.phase = phase;
            } else if constexpr (std::is_same_v<T, GainStep>) {
                const float* in = scratch_buffers.channel(s.input_buffer_slot);
                float* out = scratch_buffers.channel(s.output_buffer_slot);
                const float gain = s.linear_gain;

                for (uint32_t i = 0; i < n_frames; ++i) {
                    out[i] = in[i] * gain;
                }
            } else if constexpr (std::is_same_v<T, RouteStep>) {
                if (s.destination_channel < output_block.num_channels()) {
                    const float* src = scratch_buffers.channel(s.source_buffer_slot);
                    float* dst = output_block.channel(s.destination_channel);
                    std::memcpy(dst, src, n_frames * sizeof(float));
                }
            }
        }, step);
    }
}

} // namespace saudade::renderplan
