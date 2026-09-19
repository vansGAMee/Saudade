#include <saudade/audio/engine.hpp>
#include <saudade/audio/allocation_guard.hpp>
#include <cassert>
#include <thread>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <bit>

namespace saudade::audio {

AudioEngine::AudioEngine(std::unique_ptr<const renderplan::RenderPlan> initial_plan,
                         uint32_t max_block_size)
    : max_block_size_(max_block_size) {
    assert(initial_plan != nullptr);
    publisher_.publish(std::move(initial_plan), max_block_size_);
}

void AudioEngine::prepare(uint32_t max_block_size) {
    max_block_size_ = max_block_size;
    publisher_.prepare(max_block_size_);
}

void AudioEngine::reset() {
    publisher_.reset_active_dsp_state();
    event_queue_.clear();
    event_queue_.reset_schedule_tracking(0);
    clear_track_events();
    metronome_state_.reset();
    const uint64_t req = flush_requested_.load(std::memory_order_relaxed);
    flush_acknowledged_.store(req, std::memory_order_relaxed);
}

void AudioEngine::set_track_events(const std::vector<events::TimelineEvent>& events) noexcept {
    const uint32_t inactive_idx = 1 - active_track_idx_.load(std::memory_order_relaxed);
    auto& buf = track_buffers_[inactive_idx];
    const size_t n = std::min(events.size(), EventTrackBuffer::kMaxEvents);
    for (size_t i = 0; i < n; ++i) {
        buf.events[i] = events[i];
    }
    buf.count = n;
    std::sort(buf.events.begin(), buf.events.begin() + n);
    active_track_idx_.store(inactive_idx, std::memory_order_release);
}

void AudioEngine::clear_track_events() noexcept {
    const uint32_t inactive_idx = 1 - active_track_idx_.load(std::memory_order_relaxed);
    track_buffers_[inactive_idx].count = 0;
    active_track_idx_.store(inactive_idx, std::memory_order_release);
}

void AudioEngine::set_metronome_enabled(bool enabled) noexcept {
    metronome_enabled_.store(enabled, std::memory_order_release);
}

bool AudioEngine::is_metronome_enabled() const noexcept {
    return metronome_enabled_.load(std::memory_order_acquire);
}

void AudioEngine::set_metronome_volume(float vol) noexcept {
    metronome_volume_.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_release);
}

float AudioEngine::metronome_volume() const noexcept {
    return metronome_volume_.load(std::memory_order_acquire);
}

void AudioEngine::audition_note_on(double pitch, float velocity) noexcept {
    audition_pitch_.store(pitch, std::memory_order_release);
    audition_velocity_.store(velocity, std::memory_order_release);
    audition_trigger_.fetch_add(1, std::memory_order_release);
}

void AudioEngine::audition_note_off() noexcept {
    audition_release_.fetch_add(1, std::memory_order_release);
}

void AudioEngine::set_master_gain_db(float gain_db) noexcept {
    const float clamped = std::clamp(gain_db, -60.0f, 12.0f);
    const float linear = std::pow(10.0f, clamped / 20.0f);
    master_gain_bits_.store(std::bit_cast<uint32_t>(linear),
                            std::memory_order_release);
}

float AudioEngine::master_gain_db() const noexcept {
    const float linear = std::bit_cast<float>(
        master_gain_bits_.load(std::memory_order_acquire));
    return 20.0f * std::log10(std::max(linear, 0.000001f));
}

AudioEngine::TelemetrySnapshot AudioEngine::telemetry() const noexcept {
    TelemetrySnapshot snapshot;
    snapshot.peak_left = std::bit_cast<float>(
        peak_left_bits_.load(std::memory_order_acquire));
    snapshot.peak_right = std::bit_cast<float>(
        peak_right_bits_.load(std::memory_order_acquire));
    snapshot.rms_left = std::bit_cast<float>(
        rms_left_bits_.load(std::memory_order_acquire));
    snapshot.rms_right = std::bit_cast<float>(
        rms_right_bits_.load(std::memory_order_acquire));
    snapshot.sequence = telemetry_sequence_.load(std::memory_order_acquire);
    return snapshot;
}

void AudioEngine::publish_silence_telemetry() noexcept {
    peak_left_bits_.store(0, std::memory_order_relaxed);
    peak_right_bits_.store(0, std::memory_order_relaxed);
    rms_left_bits_.store(0, std::memory_order_relaxed);
    rms_right_bits_.store(0, std::memory_order_relaxed);
    telemetry_sequence_.fetch_add(1, std::memory_order_release);
}

void AudioEngine::apply_master_gain_and_publish_telemetry(
    AudioBlock& block) noexcept {
    const float gain = std::bit_cast<float>(
        master_gain_bits_.load(std::memory_order_relaxed));
    float peaks[2]{0.0f, 0.0f};
    double sums[2]{0.0, 0.0};
    const size_t channels = std::min<size_t>(2, block.num_channels());
    for (size_t channel = 0; channel < channels; ++channel) {
        float* samples = block.channel(channel);
        for (uint32_t frame = 0; frame < block.num_frames(); ++frame) {
            const float sample = samples[frame] * gain;
            samples[frame] = sample;
            peaks[channel] = std::max(peaks[channel], std::abs(sample));
            sums[channel] += static_cast<double>(sample) * sample;
        }
    }
    const float rms_left = block.num_frames() > 0
        ? static_cast<float>(std::sqrt(sums[0] / block.num_frames())) : 0.0f;
    const float rms_right = block.num_frames() > 0
        ? static_cast<float>(std::sqrt(sums[1] / block.num_frames())) : 0.0f;
    peak_left_bits_.store(std::bit_cast<uint32_t>(peaks[0]),
                          std::memory_order_relaxed);
    peak_right_bits_.store(std::bit_cast<uint32_t>(peaks[1]),
                           std::memory_order_relaxed);
    rms_left_bits_.store(std::bit_cast<uint32_t>(rms_left),
                         std::memory_order_relaxed);
    rms_right_bits_.store(std::bit_cast<uint32_t>(rms_right),
                          std::memory_order_relaxed);
    telemetry_sequence_.fetch_add(1, std::memory_order_release);
}

PlanGeneration AudioEngine::publish_plan(std::unique_ptr<const renderplan::RenderPlan> new_plan) {
    return publisher_.publish(std::move(new_plan), max_block_size_);
}

size_t AudioEngine::collect_retired() {
    return publisher_.collect_retired();
}

uint64_t AudioEngine::request_event_flush() noexcept {
    const uint64_t gen = ++next_flush_request_;
    flush_requested_.store(gen, std::memory_order_release);
    return gen;
}

bool AudioEngine::is_flush_acknowledged(uint64_t generation) const noexcept {
    return flush_acknowledged_.load(std::memory_order_acquire) >= generation;
}

bool AudioEngine::wait_for_flush(uint64_t generation, std::chrono::milliseconds timeout) noexcept {
    const auto start = std::chrono::steady_clock::now();
    while (!is_flush_acknowledged(generation)) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start) > timeout) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

bool AudioEngine::flush_events(std::chrono::milliseconds timeout) noexcept {
    const uint64_t gen = request_event_flush();
    if (!wait_for_flush(gen, timeout)) {
        return false;
    }
    event_queue_.reset_schedule_tracking(0);
    return true;
}

void AudioEngine::process(AudioBlock& output_block, const ProcessContext& ctx) noexcept {
    // Realtime allocation guard active during execution
    ScopedRealtimeGuard rt_guard;

    if (ctx.num_frames == 0 || ctx.num_frames > max_block_size_) {
        return;
    }

    // Capture transport state snapshot for this quantum
    const time::TransportSnapshot transport_snap = transport_.acquire_snapshot();

    // Law 3: Snapshot active plan EXACTLY ONCE per audio quantum
    PreparedPlan* const current = publisher_.acquire_current_for_quantum();

    // RT-Safe Event Flush Handshake:
    const uint64_t req = flush_requested_.load(std::memory_order_acquire);
    const uint64_t ack = flush_acknowledged_.load(std::memory_order_relaxed);
    if (req > ack) {
        while (event_queue_.pop()) {}
        quantum_event_block_.clear();
        clear_track_events();
        metronome_state_.reset();
        if (current) {
            current->dsp_state.reset();
        }
        flush_acknowledged_.store(req, std::memory_order_release);
    }

    if (!current) {
        output_block.clear();
        publish_silence_telemetry();
        return;
    }

    // Check Live Audition Triggers
    const uint64_t aud_trig = audition_trigger_.load(std::memory_order_acquire);
    const uint64_t aud_rel = audition_release_.load(std::memory_order_acquire);
    bool has_audition_event = false;

    quantum_event_block_.clear();

    if (aud_trig != last_audition_trigger_) {
        last_audition_trigger_ = aud_trig;
        const double pitch = audition_pitch_.load(std::memory_order_relaxed);
        const float vel = audition_velocity_.load(std::memory_order_relaxed);
        quantum_event_block_.push_back(events::TimedEvent(0, events::NoteOn{kAuditionNoteId, pitch, vel}));
        audition_active_ = true;
        has_audition_event = true;
    }

    if (aud_rel != last_audition_release_) {
        last_audition_release_ = aud_rel;
        if (audition_active_) {
            quantum_event_block_.push_back(events::TimedEvent(0, events::NoteOff{kAuditionNoteId, 0.0f}));
            audition_active_ = false;
            has_audition_event = true;
        }
    }

    // If transport is NOT playing:
    if (!transport_snap.playing) {
        // Any active non-audition notes that were sustaining when stopped enter release
        for (size_t s_idx = 0; s_idx < current->plan->num_synth_states(); ++s_idx) {
            auto& syn = current->dsp_state.synth_state(s_idx);
            for (auto& v : syn.voices) {
                if (v.active && v.note_id != kAuditionNoteId &&
                    v.env_stage != renderplan::EnvelopeStage::Idle &&
                    v.env_stage != renderplan::EnvelopeStage::Release) {
                    v.env_stage = renderplan::EnvelopeStage::Release;
                }
            }
        }

        bool any_voice_active = audition_active_ || has_audition_event;
        if (!any_voice_active) {
            for (size_t s_idx = 0; s_idx < current->plan->num_synth_states(); ++s_idx) {
                const auto& syn = current->dsp_state.synth_state(s_idx);
                for (const auto& v : syn.voices) {
                    if (v.active) {
                        any_voice_active = true;
                        break;
                    }
                }
                if (any_voice_active) break;
            }
        }

        if (!any_voice_active) {
            output_block.clear();
            publish_silence_telemetry();
            publisher_.acknowledge_completed_generation(current->generation);
            return;
        }

        ProcessContext local_ctx = ctx;
        local_ctx.block_start_sample = transport_snap.block_start_sample;
        local_ctx.transport_playing = false;

        quantum_event_block_.sort();
        current->plan->render(output_block, current->scratch_buffers, current->dsp_state, local_ctx, quantum_event_block_.view());
        apply_master_gain_and_publish_telemetry(output_block);
        publisher_.acknowledge_completed_generation(current->generation);
        return;
    }

    // Transport IS PLAYING
    ProcessContext local_ctx = ctx;
    local_ctx.block_start_sample = transport_snap.block_start_sample;
    local_ctx.transport_playing = true;

    const time::SamplePosition start_pos = transport_snap.block_start_sample;
    const auto num_frames = ctx.num_frames;
    const time::SamplePosition end_pos = start_pos + static_cast<time::SamplePosition>(num_frames);

    const bool loop_active = transport_snap.loop_enabled &&
                             (transport_snap.loop_end_sample > transport_snap.loop_start_sample);

    const auto& track_buf = track_buffers_[active_track_idx_.load(std::memory_order_acquire)];

    // Looping or standard quantum
    if (loop_active && end_pos > transport_snap.loop_end_sample) {
        const auto boundary_offset = static_cast<uint32_t>(transport_snap.loop_end_sample - start_pos);

        // Segment A: [start_pos, loop_end_sample)
        for (size_t i = 0; i < track_buf.count; ++i) {
            const auto& ev = track_buf.events[i];
            if (ev.sample_position >= start_pos && ev.sample_position < transport_snap.loop_end_sample) {
                const auto off = static_cast<uint32_t>(ev.sample_position - start_pos);
                quantum_event_block_.push_back(events::TimedEvent(off, ev.payload));
            }
        }

        // Segment B: wrapped timeline
        const auto wrapped_frames = num_frames - boundary_offset;
        const time::SamplePosition wrapped_end = transport_snap.loop_start_sample + static_cast<time::SamplePosition>(wrapped_frames);
        for (size_t i = 0; i < track_buf.count; ++i) {
            const auto& ev = track_buf.events[i];
            if (ev.sample_position >= transport_snap.loop_start_sample && ev.sample_position < wrapped_end) {
                const auto off = boundary_offset + static_cast<uint32_t>(ev.sample_position - transport_snap.loop_start_sample);
                if (off < num_frames) {
                    quantum_event_block_.push_back(events::TimedEvent(off, ev.payload));
                }
            }
        }
    } else {
        // Normal non-wrapping quantum
        for (size_t i = 0; i < track_buf.count; ++i) {
            const auto& ev = track_buf.events[i];
            if (ev.sample_position >= start_pos && ev.sample_position < end_pos) {
                const auto off = static_cast<uint32_t>(ev.sample_position - start_pos);
                quantum_event_block_.push_back(events::TimedEvent(off, ev.payload));
            }
        }
    }

    // Also drain from event_queue_ (for unit tests and live events)
    events::TimelineEvent q_ev{};
    while (event_queue_.peek(q_ev)) {
        if (q_ev.sample_position < start_pos) {
            event_queue_.pop();
            continue;
        }
        if (q_ev.sample_position >= end_pos) {
            break;
        }
        event_queue_.pop();
        const auto off = static_cast<uint32_t>(q_ev.sample_position - start_pos);
        quantum_event_block_.push_back(events::TimedEvent(off, q_ev.payload));
    }

    quantum_event_block_.sort();

    // Render quantum
    current->plan->render(output_block, current->scratch_buffers, current->dsp_state, local_ctx, quantum_event_block_.view());

    // Metronome Rendering
    if (metronome_enabled_.load(std::memory_order_acquire)) {
        const double sr = ctx.sample_rate > 0.0 ? ctx.sample_rate : 48000.0;
        const auto& tmap = transport_.tempo_map();
        constexpr float two_pi = static_cast<float>(2.0 * std::numbers::pi_v<double>);
        const float met_vol = metronome_volume_.load(std::memory_order_relaxed);

        auto check_beats = [&](time::SamplePosition s_start, time::SamplePosition s_end) {
            const auto beat_a = tmap.sample_to_beat(s_start, sr);
            const auto beat_b = tmap.sample_to_beat(s_end, sr);
            const int64_t b0 = beat_a.ticks / time::BeatPosition::kTicksPerBeat;
            const int64_t b1 = (beat_b.ticks + time::BeatPosition::kTicksPerBeat - 1) / time::BeatPosition::kTicksPerBeat;

            for (int64_t b = b0; b <= b1; ++b) {
                const auto b_sample = tmap.beat_to_sample(time::BeatPosition::from_beats(b), sr);
                if (b_sample >= s_start && b_sample < s_end && b != metronome_state_.last_beat) {
                    metronome_state_.last_beat = b;
                    const float freq = (b % 4 == 0) ? 2200.0f : 1200.0f;
                    metronome_state_.active = true;
                    metronome_state_.phase = 0.0f;
                    metronome_state_.phase_step = static_cast<float>(2.0 * std::numbers::pi_v<double> * freq / sr);
                    metronome_state_.envelope = (b % 4 == 0) ? 1.0f : 0.65f;
                }
            }
        };

        if (loop_active && end_pos > transport_snap.loop_end_sample) {
            const auto boundary_offset = static_cast<uint32_t>(transport_snap.loop_end_sample - start_pos);
            check_beats(start_pos, transport_snap.loop_end_sample);
            const auto wrapped_end = transport_snap.loop_start_sample + static_cast<time::SamplePosition>(num_frames - boundary_offset);
            check_beats(transport_snap.loop_start_sample, wrapped_end);
        } else {
            check_beats(start_pos, end_pos);
        }

        if (metronome_state_.active) {
            const size_t num_ch = output_block.num_channels();
            for (uint32_t i = 0; i < num_frames; ++i) {
                if (metronome_state_.envelope > 0.0005f) {
                    const float click = std::sin(metronome_state_.phase) * (metronome_state_.envelope * met_vol);
                    metronome_state_.phase += metronome_state_.phase_step;
                    if (metronome_state_.phase >= two_pi) metronome_state_.phase -= two_pi;
                    metronome_state_.envelope *= metronome_state_.decay;

                    for (size_t ch = 0; ch < num_ch; ++ch) {
                        output_block.channel(ch)[i] += click;
                    }
                } else {
                    metronome_state_.active = false;
                    metronome_state_.envelope = 0.0f;
                    break;
                }
            }
        }
    }

    apply_master_gain_and_publish_telemetry(output_block);
    publisher_.acknowledge_completed_generation(current->generation);
    transport_.advance_quantum(ctx.num_frames);
}

} // namespace saudade::audio
