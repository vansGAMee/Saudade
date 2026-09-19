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
                float* out_left = scratch_buffers.channel(s.output_left_buffer_slot);
                float* out_right = scratch_buffers.channel(s.output_right_buffer_slot);
                PolySynthState& state = dsp_state.synth_state(s.state_index);

                const float attack_time = s.attack_seconds;
                const float decay_time = s.decay_seconds;
                const float sustain_level = s.sustain;
                const float release_time = s.release_seconds;
                const float base_cutoff = s.cutoff_hz;
                const float resonance = s.resonance;
                const float character = s.character;

                const float attack_step = 1.0f / (attack_time * static_cast<float>(sr));
                const float decay_step = (1.0f - sustain_level) / (decay_time * static_cast<float>(sr));
                const float release_step = 1.0f / (release_time * static_cast<float>(sr));
                const float max_cutoff = static_cast<float>(sr) * 0.45f;
                const float pi_over_sr = static_cast<float>(std::numbers::pi_v<double> / sr);

                auto poly_blep = [](float t, float dt) noexcept -> float {
                    if (t < dt) {
                        const float t_div = t / dt;
                        return t_div + t_div - t_div * t_div - 1.0f;
                    } else if (t > 1.0f - dt) {
                        const float t_div = (t - 1.0f) / dt;
                        return t_div * t_div + t_div + t_div + 1.0f;
                    }
                    return 0.0f;
                };

                auto render_interval = [&](uint32_t start_frame, uint32_t end_frame) noexcept {
                    if (start_frame >= end_frame) {
                        return;
                    }
                    std::memset(out_left + start_frame, 0,
                                (end_frame - start_frame) * sizeof(float));
                    std::memset(out_right + start_frame, 0,
                                (end_frame - start_frame) * sizeof(float));

                    for (auto& voice : state.voices) {
                        if (!voice.active || voice.env_stage == EnvelopeStage::Idle) {
                            continue;
                        }

                        const float dt = static_cast<float>(voice.frequency / sr);
                        const float dt_sub = 0.5f * dt;
                        float phase = voice.phase;
                        float sub_phase = voice.sub_phase;
                        EnvelopeStage env_stage = voice.env_stage;
                        float env_level = voice.env_level;
                        const float vel = voice.velocity;
                        const float voice_gain = voice.gain;
                        const float pan = std::clamp(voice.pan, -1.0f, 1.0f);
                        const float pan_angle =
                            (pan + 1.0f) * static_cast<float>(std::numbers::pi_v<double> * 0.25);
                        const float left_gain = std::cos(pan_angle) * voice_gain;
                        const float right_gain = std::sin(pan_angle) * voice_gain;
                        auto svf = voice.svf;

                        for (uint32_t i = start_frame; i < end_frame; ++i) {
                            switch (env_stage) {
                            case EnvelopeStage::Attack:
                                env_level += attack_step;
                                if (env_level >= 1.0f) {
                                    env_level = 1.0f;
                                    env_stage = EnvelopeStage::Decay;
                                }
                                break;
                            case EnvelopeStage::Decay:
                                env_level -= decay_step;
                                if (env_level <= sustain_level) {
                                    env_level = sustain_level;
                                    env_stage = EnvelopeStage::Sustain;
                                }
                                break;
                            case EnvelopeStage::Sustain:
                                env_level = sustain_level;
                                break;
                            case EnvelopeStage::Release:
                                env_level -= release_step;
                                if (env_level <= 0.0001f) {
                                    env_level = 0.0f;
                                    env_stage = EnvelopeStage::Idle;
                                }
                                break;
                            case EnvelopeStage::Idle:
                                break;
                            }

                            if (env_stage == EnvelopeStage::Idle) {
                                break;
                            }

                            // Bandlimited PolyBLEP saw
                            float saw = 2.0f * phase - 1.0f;
                            saw -= poly_blep(phase, dt);
                            const float triangle =
                                1.0f - 4.0f * std::abs(phase - 0.5f);

                            // Warm sub-oscillator sine
                            const float sub = std::sin(two_pi * sub_phase);

                            // Advance phases
                            phase += dt;
                            if (phase >= 1.0f) phase -= 1.0f;
                            sub_phase += dt_sub;
                            if (sub_phase >= 1.0f) sub_phase -= 1.0f;

                            // Oscillator sum shaped by envelope and velocity
                            const float primary =
                                triangle * (1.0f - character) + saw * character;
                            const float osc =
                                (primary + 0.22f * sub) * (env_level * vel);

                            // Cytomic 2-pole SVF filter modulated by envelope and velocity
                            const float cutoff_hz = std::clamp(
                                base_cutoff * (0.45f + 0.75f * env_level * vel),
                                40.0f, max_cutoff);
                            const float g = std::tan(pi_over_sr * cutoff_hz);
                            const float k_damp = 1.05f - resonance * 0.65f;
                            const float a1 = 1.0f / (1.0f + g * (g + k_damp));
                            const float a2 = g * a1;
                            const float a3 = g * a2;

                            const float v3 = osc - svf.s2;
                            const float v1 = a1 * svf.s1 + a2 * v3;
                            const float v2 = svf.s2 + a2 * svf.s1 + a3 * v3;
                            svf.s1 = 2.0f * v1 - svf.s1;
                            svf.s2 = 2.0f * v2 - svf.s2;

                            const float sample = v2 * 0.45f;
                            out_left[i] += sample * left_gain;
                            out_right[i] += sample * right_gain;
                        }

                        voice.phase = phase;
                        voice.sub_phase = sub_phase;
                        voice.env_stage = env_stage;
                        voice.env_level = env_level;
                        voice.svf = svf;
                        if (env_stage == EnvelopeStage::Idle) {
                            voice.active = false;
                            voice.svf.reset();
                        }
                    }
                };

                auto apply_event = [&](const events::TimedEvent& ev) noexcept {
                    if (std::holds_alternative<events::NoteOn>(ev.payload)) {
                        const auto& on = std::get<events::NoteOn>(ev.payload);
                        size_t chosen = PolySynthState::kVoiceCount;

                        // 1. First free / idle voice
                        for (size_t v = 0; v < PolySynthState::kVoiceCount; ++v) {
                            if (!state.voices[v].active || state.voices[v].env_stage == EnvelopeStage::Idle) {
                                chosen = v;
                                break;
                            }
                        }

                        // 2. Releasing voice with lowest envelope level
                        if (chosen == PolySynthState::kVoiceCount) {
                            float min_level = 2.0f;
                            for (size_t v = 0; v < PolySynthState::kVoiceCount; ++v) {
                                if (state.voices[v].env_stage == EnvelopeStage::Release &&
                                    state.voices[v].env_level < min_level) {
                                    min_level = state.voices[v].env_level;
                                    chosen = v;
                                }
                            }
                        }

                        // 3. Oldest active voice (minimum age)
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
                            voice.gain = std::clamp(on.gain, 0.0f, 4.0f);
                            voice.pan = std::clamp(on.pan, -1.0f, 1.0f);
                            voice.phase = 0.0f;
                            voice.sub_phase = 0.0f;
                            voice.env_stage = EnvelopeStage::Attack;
                            voice.env_level = 0.0f;
                            voice.svf.reset();
                            voice.age = ++state.next_voice_age;
                        }
                    } else if (std::holds_alternative<events::NoteOff>(ev.payload)) {
                        const auto& off = std::get<events::NoteOff>(ev.payload);
                        for (auto& voice : state.voices) {
                            if (voice.active && voice.note_id == off.note_id) {
                                voice.env_stage = EnvelopeStage::Release;
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
