#include <saudade/renderplan/render_plan.hpp>
#include <cmath>
#include <cstring>
#include <numbers>

namespace saudade::renderplan {

RenderPlan::RenderPlan(std::vector<ExecutionStep> steps,
                       uint32_t num_scratch_buffers,
                       size_t num_sine_states,
                       size_t output_channels,
                       size_t num_synth_states)
    : steps_(std::move(steps)),
      num_scratch_buffers_(num_scratch_buffers),
      num_sine_states_(num_sine_states),
      num_synth_states_(num_synth_states),
      output_channels_(output_channels) {}

DspStateStorage RenderPlan::create_dsp_state() const {
    DspStateStorage storage;
    storage.allocate_sine_states(num_sine_states_);
    storage.allocate_synth_states(num_synth_states_);
    return storage;
}

void RenderPlan::render(audio::AudioBlock& output_block,
                        audio::AudioBuffer& scratch_buffers,
                        DspStateStorage& dsp_state,
                        const audio::ProcessContext& ctx,
                        events::EventBlockView events) const noexcept {
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
            } else if constexpr (std::is_same_v<T, PolySynthStep>) {
                float* out = scratch_buffers.channel(s.output_buffer_slot);
                PolySynthState& state = dsp_state.synth_state(s.state_index);

                auto render_interval = [&](uint32_t start_frame, uint32_t end_frame) noexcept {
                    if (start_frame >= end_frame) {
                        return;
                    }
                    std::memset(out + start_frame, 0, (end_frame - start_frame) * sizeof(float));

                    for (auto& voice : state.voices) {
                        if (!voice.active) {
                            continue;
                        }
                        const float phase_step = static_cast<float>(
                            (2.0 * std::numbers::pi_v<double> * voice.frequency) / sr
                        );
                        float phase = voice.phase;
                        const float vel = voice.velocity;

                        for (uint32_t i = start_frame; i < end_frame; ++i) {
                            out[i] += std::sin(phase) * vel;
                            phase += phase_step;
                            if (phase >= two_pi) {
                                phase -= two_pi;
                            }
                        }
                        voice.phase = phase;
                    }
                };

                auto apply_event = [&](const events::TimedEvent& ev) noexcept {
                    if (std::holds_alternative<events::NoteOn>(ev.payload)) {
                        const auto& on = std::get<events::NoteOn>(ev.payload);
                        size_t chosen = PolySynthState::kVoiceCount;

                        // 1. First free voice (lowest index)
                        for (size_t v = 0; v < PolySynthState::kVoiceCount; ++v) {
                            if (!state.voices[v].active) {
                                chosen = v;
                                break;
                            }
                        }

                        // 2. Deterministic voice stealing: oldest active voice (minimum age)
                        if (chosen == PolySynthState::kVoiceCount) {
                            uint64_t min_age = UINT64_MAX;
                            for (size_t v = 0; v < PolySynthState::kVoiceCount; ++v) {
                                if (state.voices[v].age < min_age) {
                                    min_age = state.voices[v].age;
                                    chosen = v;
                                }
                            }
                        }

                        if (chosen < PolySynthState::kVoiceCount) {
                            auto& voice = state.voices[chosen];
                            voice.active = true;
                            voice.note_id = on.note_id;
                            voice.pitch = on.pitch;
                            voice.frequency = 440.0 * std::exp2((on.pitch - 69.0) / 12.0);
                            voice.velocity = on.velocity;
                            voice.phase = 0.0f;
                            voice.age = ++state.next_voice_age;
                        }
                    } else if (std::holds_alternative<events::NoteOff>(ev.payload)) {
                        const auto& off = std::get<events::NoteOff>(ev.payload);
                        for (auto& voice : state.voices) {
                            if (voice.active && voice.note_id == off.note_id) {
                                voice.active = false;
                            }
                        }
                    }
                };

                uint32_t curr_frame = 0;
                const size_t num_events = events.size();

                for (size_t ev_idx = 0; ev_idx < num_events; ) {
                    if (events[ev_idx].sample_offset >= n_frames) {
                        break;
                    }
                    const uint32_t ev_offset = events[ev_idx].sample_offset;

                    if (ev_offset > curr_frame) {
                        render_interval(curr_frame, ev_offset);
                        curr_frame = ev_offset;
                    }

                    while (ev_idx < num_events && events[ev_idx].sample_offset == ev_offset) {
                        apply_event(events[ev_idx]);
                        ++ev_idx;
                    }
                }

                if (curr_frame < n_frames) {
                    render_interval(curr_frame, n_frames);
                }
            }
        }, step);
    }
}

} // namespace saudade::renderplan
