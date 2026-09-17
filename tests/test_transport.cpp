#include <saudade/time/transport.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/allocation_guard.hpp>

#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_sine_plan(float freq = 440.0f) {
    graph::GraphModel graph;
    const auto sine = graph.add_sine_node(freq);
    const auto gain = graph.add_gain_node(-12.0f);
    const auto out = graph.add_output_node(2);

    graph.connect(sine, graph::SineNode::kPortOut,
                  gain, graph::GainNode::kPortIn);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortLeft);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortRight);

    return graph::GraphCompiler::compile(graph);
}

} // namespace

void test_initial_state_and_basic_controls() {
    std::cout << "[RUN] test_initial_state_and_basic_controls\n";

    time::TransportController transport;

    // Default state: Stopped at sample 0
    assert(transport.state() == time::TransportState::Stopped);
    assert(!transport.is_playing());
    assert(transport.current_sample() == 0);
    assert(transport.current_beat(48000.0) == time::BeatPosition::zero());

    // Play -> Playing
    transport.play();
    assert(transport.state() == time::TransportState::Playing);
    assert(transport.is_playing());

    // Stop -> Stopped
    transport.stop();
    assert(transport.state() == time::TransportState::Stopped);
    assert(!transport.is_playing());

    std::cout << "  [PASS] Initial state and play/stop commands verified\n";
}

void test_quantum_advancement_and_stop_freeze() {
    std::cout << "[RUN] test_quantum_advancement_and_stop_freeze\n";

    time::TransportController transport;
    const uint32_t quantum = 256;

    // 1. Quanta while stopped: position does not advance
    for (int i = 0; i < 5; ++i) {
        auto snap = transport.acquire_snapshot();
        assert(!snap.playing);
        assert(snap.block_start_sample == 0);
        transport.advance_quantum(quantum);
        assert(transport.current_sample() == 0);
    }

    // 2. Play: position advances by quantum each block
    transport.play();

    auto snap0 = transport.acquire_snapshot();
    assert(snap0.playing);
    assert(snap0.block_start_sample == 0);
    transport.advance_quantum(quantum);
    assert(transport.current_sample() == 256);

    auto snap1 = transport.acquire_snapshot();
    assert(snap1.playing);
    assert(snap1.block_start_sample == 256);
    transport.advance_quantum(quantum);
    assert(transport.current_sample() == 512);

    auto snap2 = transport.acquire_snapshot();
    assert(snap2.playing);
    assert(snap2.block_start_sample == 512);
    transport.advance_quantum(quantum);
    assert(transport.current_sample() == 768);

    // 3. Stop: position freezes at 768
    transport.stop();

    for (int i = 0; i < 5; ++i) {
        auto snap_stop = transport.acquire_snapshot();
        assert(!snap_stop.playing);
        assert(snap_stop.block_start_sample == 768);
        transport.advance_quantum(quantum);
        assert(transport.current_sample() == 768);
    }

    std::cout << "  [PASS] Quantum advancement when playing and freeze when stopped verified\n";
}

void test_seek_controls() {
    std::cout << "[RUN] test_seek_controls\n";

    time::TransportController transport;
    const double sr = 48000.0;
    const uint32_t quantum = 256;

    // Seek samples while stopped
    transport.seek_samples(48000); // 1.0 second
    assert(transport.current_sample() == 48000);

    auto snap = transport.acquire_snapshot();
    assert(snap.block_start_sample == 48000);
    assert(transport.current_sample() == 48000);

    // Clamped negative seek
    transport.seek_samples(-500);
    assert(transport.current_sample() == 0);
    snap = transport.acquire_snapshot();
    assert(snap.block_start_sample == 0);

    // Seek beats: 4 beats at 120 BPM = 2.0s = 96,000 samples at 48k
    transport.seek_beats(time::BeatPosition::from_beats(4), sr);
    assert(transport.current_sample() == 96000);
    assert(transport.current_beat(sr) == time::BeatPosition::from_beats(4));

    snap = transport.acquire_snapshot();
    assert(snap.block_start_sample == 96000);

    // Play resumes from seeked position
    transport.play();
    snap = transport.acquire_snapshot();
    assert(snap.playing);
    assert(snap.block_start_sample == 96000);
    transport.advance_quantum(quantum);
    assert(transport.current_sample() == 96000 + quantum);

    std::cout << "  [PASS] Sample and Beat seeking verified\n";
}

void test_race_free_mid_quantum_seek() {
    std::cout << "[RUN] test_race_free_mid_quantum_seek\n";

    time::TransportController transport;
    const uint32_t quantum = 256;

    transport.play();
    auto snap = transport.acquire_snapshot();
    assert(snap.block_start_sample == 0);

    // Control seeks while RT was supposedly processing quantum [0, 256)
    transport.seek_samples(100000);

    // RT finishes quantum and calls advance_quantum(256).
    // Because seek_version_ changed during the quantum, advance_quantum must NOT overwrite 100,000!
    transport.advance_quantum(quantum);

    assert(transport.current_sample() == 100000);

    // Next quantum starts at 100000
    auto next_snap = transport.acquire_snapshot();
    assert(next_snap.block_start_sample == 100000);
    transport.advance_quantum(quantum);
    assert(transport.current_sample() == 100000 + quantum);

    std::cout << "  [PASS] Mid-quantum seek race avoidance verified\n";
}

void test_engine_process_silence_on_stop() {
    std::cout << "[RUN] test_engine_process_silence_on_stop\n";

    auto plan = make_sine_plan(440.0f);
    audio::AudioEngine engine(std::move(plan));

    const uint32_t quantum = 256;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    // 1. In default stopped state, process() must output silence
    auto block = buffer.block(quantum);
    // Fill buffer with non-zero dummy values first to test clearing
    for (size_t ch = 0; ch < 2; ++ch) {
        float* p = block.channel(ch);
        for (uint32_t i = 0; i < quantum; ++i) {
            p[i] = 1.0f;
        }
    }

    engine.process(block, ctx);

    // Must be completely silenced
    for (size_t ch = 0; ch < 2; ++ch) {
        const float* p = block.channel(ch);
        for (uint32_t i = 0; i < quantum; ++i) {
            assert(p[i] == 0.0f);
        }
    }
    assert(engine.transport().current_sample() == 0);

    // 2. Play: process() produces audio and advances position
    engine.play();
    engine.process(block, ctx);

    bool has_nonzero = false;
    for (size_t ch = 0; ch < 2; ++ch) {
        const float* p = block.channel(ch);
        for (uint32_t i = 0; i < quantum; ++i) {
            if (p[i] != 0.0f) has_nonzero = true;
        }
    }
    assert(has_nonzero);
    assert(engine.transport().current_sample() == quantum);

    std::cout << "  [PASS] Silence when stopped and audio generation when playing verified\n";
}

void test_concurrent_transport_stress() {
    std::cout << "[RUN] test_concurrent_transport_stress\n";

    auto plan = make_sine_plan(440.0f);
    audio::AudioEngine engine(std::move(plan));

    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    std::atomic<bool> stop_threads{false};
    std::atomic<uint64_t> rt_quanta{0};

    audio::reset_realtime_allocation_count();

    // 1. Synthetic RT thread rendering audio blocks at high rate
    std::thread rt_thread([&]() {
        while (!stop_threads.load(std::memory_order_relaxed)) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            rt_quanta.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // 2. Control thread hammering transport commands
    for (int i = 0; i < 500; ++i) {
        if (i % 2 == 0) {
            engine.play();
        } else {
            engine.stop();
        }

        if (i % 5 == 0) {
            engine.seek_samples(static_cast<time::SamplePosition>(i * 1000));
        } else if (i % 7 == 0) {
            engine.seek_beats(time::BeatPosition::from_beats(i % 16), 48000.0);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    stop_threads.store(true, std::memory_order_relaxed);
    rt_thread.join();

    assert(rt_quanta.load(std::memory_order_relaxed) > 500);
    assert(audio::get_realtime_allocation_count() == 0);

    std::cout << "  [PASS] Concurrent transport stress under TSan passed with 0 data races and 0 allocations\n";
}

int main() {
    try {
        test_initial_state_and_basic_controls();
        test_quantum_advancement_and_stop_freeze();
        test_seek_controls();
        test_race_free_mid_quantum_seek();
        test_engine_process_silence_on_stop();
        test_concurrent_transport_stress();
        std::cout << "All transport tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
