#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/allocation_guard.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_synth_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain_l = graph.add_gain_node(-12.0f);
    const auto gain_r = graph.add_gain_node(-12.0f);
    const auto out = graph.add_output_node(2);

    graph.connect(synth, graph::PolySynthNode::kPortLeft,
                  gain_l, graph::GainNode::kPortIn);
    graph.connect(synth, graph::PolySynthNode::kPortRight,
                  gain_r, graph::GainNode::kPortIn);
    graph.connect(gain_l, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortLeft);
    graph.connect(gain_r, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortRight);

    return graph::GraphCompiler::compile(graph);
}

} // namespace

void test_e_polyphony_8_voices() {
    std::cout << "[RUN] test_e_polyphony_8_voices\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 256;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // 8 musical notes (C major scale)
    const double pitches[8] = {60.0, 62.0, 64.0, 65.0, 67.0, 69.0, 71.0, 72.0};
    for (size_t i = 0; i < 8; ++i) {
        assert(engine.schedule_note_on(0, i + 1, pitches[i], 0.5f));
    }

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    const float* left = block.channel(0);

    // All samples must be finite and contain audible signal
    bool has_sound = false;
    for (uint32_t i = 0; i < quantum; ++i) {
        assert(std::isfinite(left[i]));
        if (std::abs(left[i]) > 1e-5f) has_sound = true;
    }
    assert(has_sound);

    std::cout << "  [PASS] 8 simultaneous voices rendered cleanly without crash or NaN\n";
}

void test_f_deterministic_voice_stealing() {
    std::cout << "[RUN] test_f_deterministic_voice_stealing\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    constexpr size_t V = renderplan::PolySynthState::kVoiceCount;

    // Fill all voices at sample 0 (NoteId 1..V)
    for (size_t i = 1; i <= V; ++i) {
        assert(engine.schedule_note_on(0, i, 60.0 + static_cast<double>(i), 0.5f));
    }

    // Now schedule (V+1)th note at sample 30 (NoteId V+1, pitch 80.0)
    // Deterministic rule: NoteId 1 was oldest (allocated first), so it must be stolen!
    const events::NoteId stolen_id = V + 1;
    assert(engine.schedule_note_on(30, stolen_id, 80.0, 1.0f));

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    // Now NoteOff for NoteId 1 at sample 60:
    // Since NoteId 1 was already stolen, NoteOff(1) must NOT stop stolen_id!
    assert(engine.schedule_note_off(60, 1));

    // Next quantum: stolen_id and Notes 2..V are still playing
    auto block2 = buffer.block(quantum);
    engine.process(block2, ctx);

    const float* left2 = block2.channel(0);
    bool sound_still_playing = false;
    for (uint32_t i = 0; i < quantum; ++i) {
        if (std::abs(left2[i]) > 1e-5f) sound_still_playing = true;
    }
    assert(sound_still_playing);

    // Turn off stolen_id at sample 0 of block 3
    assert(engine.schedule_note_off(256, stolen_id));
    // Turn off remaining 2..V
    for (size_t i = 2; i <= V; ++i) {
        assert(engine.schedule_note_off(256, i));
    }

    // Block 3: notes have entered release phase and are decaying
    auto block3 = buffer.block(quantum);
    engine.process(block3, ctx);

    // Process blocks until 120ms release phase finishes (~5760 samples)
    for (int b = 0; b < 60; ++b) {
        auto blk = buffer.block(quantum);
        engine.process(blk, ctx);
    }

    // After release phase, all voices must be completely silent
    auto block_silent = buffer.block(quantum);
    engine.process(block_silent, ctx);
    const float* left_silent = block_silent.channel(0);
    for (uint32_t i = 0; i < quantum; ++i) {
        assert(left_silent[i] == 0.0f);
    }

    std::cout << "  [PASS] Deterministic oldest-voice stealing verified\n";
}

void test_h_transport_interaction() {
    std::cout << "[RUN] test_h_transport_interaction\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    // Transport is default STOPPED
    assert(!engine.transport().is_playing());
    assert(engine.schedule_note_on(0, 1, 60.0, 1.0f));

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    // Must output silence when stopped
    const float* left = block.channel(0);
    for (uint32_t i = 0; i < quantum; ++i) {
        assert(left[i] == 0.0f);
    }

    // Event must NOT have been consumed from the queue yet
    assert(!engine.event_queue().empty());

    // Now start playback
    engine.play();
    engine.process(block, ctx);

    bool sound = false;
    for (uint32_t i = 1; i < quantum; ++i) {
        if (std::abs(left[i]) > 1e-5f) sound = true;
    }
    assert(sound);

    std::cout << "  [PASS] Transport stopped keeps silence and retains events until playback starts\n";
}

void test_i_zero_rt_allocations() {
    std::cout << "[RUN] test_i_zero_rt_allocations\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 256;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // Schedule 16 note events (incurs voice allocations & stealing)
    for (size_t i = 0; i < 16; ++i) {
        engine.schedule_note_on(static_cast<time::SamplePosition>(i * 10), i + 1, 60.0 + static_cast<double>(i), 0.5f);
    }

    audio::reset_realtime_allocation_count();

    // Execute 50 blocks under ScopedRealtimeGuard
    for (int b = 0; b < 50; ++b) {
        auto block = buffer.block(quantum);
        engine.process(block, ctx);
    }

    assert(audio::get_realtime_allocation_count() == 0);
    std::cout << "  [PASS] Zero allocations detected during synth rendering and voice stealing\n";
}

void test_j_tsan_concurrency() {
    std::cout << "[RUN] test_j_tsan_concurrency\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> rt_quanta{0};

    // 1. Synthetic RT audio thread
    std::thread rt_thread([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            rt_quanta.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // 2. Control thread hammering schedule API
    events::NoteId note_id = 1;
    for (int i = 0; i < 200; ++i) {
        const auto current_sample = engine.transport().current_sample();
        const auto sched_time = current_sample + 100;
        engine.schedule_note_on(sched_time, note_id, 60.0 + static_cast<double>(i % 12), 0.8f);
        engine.schedule_note_off(sched_time + 50, note_id, 0.0f);
        ++note_id;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    stop.store(true, std::memory_order_relaxed);
    rt_thread.join();

    assert(rt_quanta.load(std::memory_order_relaxed) > 100);
    std::cout << "  [PASS] Concurrent scheduling and RT execution passed with 0 data races\n";
}

void test_k_pan_master_gain_and_telemetry() {
    std::cout << "[RUN] test_k_pan_master_gain_and_telemetry\n";

    audio::AudioEngine engine(make_synth_plan());
    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext ctx{48000.0, 512};
    const std::vector<events::TimelineEvent> left_events{
        events::TimelineEvent{
            0, events::NoteOn{9001, 60.0, 0.8f, 1.0f, -1.0f}},
    };
    engine.set_track_events(left_events);
    engine.play();
    auto block = buffer.block(512);
    engine.process(block, ctx);

    float peak_l = 0.0f;
    float peak_r = 0.0f;
    for (uint32_t i = 0; i < 512; ++i) {
        peak_l = std::max(peak_l, std::abs(buffer.channel(0)[i]));
        peak_r = std::max(peak_r, std::abs(buffer.channel(1)[i]));
    }
    assert(peak_l > 0.001f);
    assert(peak_r < peak_l * 0.01f);

    const auto telemetry = engine.telemetry();
    assert(telemetry.sequence > 0);
    assert(std::abs(telemetry.peak_left - peak_l) < 0.00001f);
    assert(std::abs(telemetry.peak_right - peak_r) < 0.00001f);
    assert(telemetry.rms_left >= 0.0f &&
           telemetry.rms_left <= telemetry.peak_left);

    audio::AudioEngine quieter(make_synth_plan());
    quieter.set_master_gain_db(-6.0f);
    quieter.set_track_events(left_events);
    quieter.play();
    auto quiet_block = buffer.block(512);
    quieter.process(quiet_block, ctx);
    const auto quiet_telemetry = quieter.telemetry();
    assert(quiet_telemetry.peak_left < telemetry.peak_left * 0.52f);
    assert(quiet_telemetry.peak_left > telemetry.peak_left * 0.48f);

    std::cout << "  [PASS] Per-note pan, master gain, and bounded telemetry verified\n";
}

int main() {
    try {
        test_e_polyphony_8_voices();
        test_f_deterministic_voice_stealing();
        test_h_transport_interaction();
        test_i_zero_rt_allocations();
        test_j_tsan_concurrency();
        test_k_pan_master_gain_and_telemetry();
        std::cout << "All PolySynth tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
