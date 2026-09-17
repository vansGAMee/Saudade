#include <saudade/events/event_types.hpp>
#include <saudade/events/event_block.hpp>
#include <saudade/events/event_queue.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_synth_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain = graph.add_gain_node(0.0f); // 0 dB unity gain for exact testing
    const auto out = graph.add_output_node(2);

    graph.connect(synth, graph::PolySynthNode::kPortOut,
                  gain, graph::GainNode::kPortIn);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortLeft);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortRight);

    return graph::GraphCompiler::compile(graph);
}

} // namespace

void test_a_event_offset_accuracy() {
    std::cout << "[RUN] test_a_event_offset_accuracy\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // Schedule NoteOn at offset 37, NoteOff at offset 91
    const bool on_ok = engine.schedule_note_on(37, /*id=*/1, /*pitch=*/69.0, /*vel=*/1.0f);
    const bool off_ok = engine.schedule_note_off(91, /*id=*/1, /*rel_vel=*/0.0f);
    assert(on_ok && off_ok);

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    const float* left = block.channel(0);

    // Verify samples [0, 37) are strictly silent
    for (uint32_t i = 0; i < 37; ++i) {
        assert(left[i] == 0.0f);
    }

    // Verify samples [37, 91) contain active synth signal
    bool has_active_signal = false;
    for (uint32_t i = 37; i < 91; ++i) {
        if (std::abs(left[i]) > 1e-6f) {
            has_active_signal = true;
        }
    }
    assert(has_active_signal);

    // Verify samples [91, 128) are strictly silent
    for (uint32_t i = 91; i < 128; ++i) {
        assert(left[i] == 0.0f);
    }

    std::cout << "  [PASS] Offset accuracy: [0, 37) silent, [37, 91) active, [91, 128) silent\n";
}

void test_b_quantum_boundaries() {
    std::cout << "[RUN] test_b_quantum_boundaries\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // Schedule NoteOn exactly at sample 128 (start of quantum 2)
    const bool ok = engine.schedule_note_on(128, /*id=*/10, /*pitch=*/60.0, /*vel=*/1.0f);
    assert(ok);

    // Quantum 1: [0, 128)
    auto block1 = buffer.block(quantum);
    engine.process(block1, ctx);
    const float* left1 = block1.channel(0);

    // Must be completely silent in quantum 1
    for (uint32_t i = 0; i < quantum; ++i) {
        assert(left1[i] == 0.0f);
    }

    // Quantum 2: [128, 256)
    auto block2 = buffer.block(quantum);
    engine.process(block2, ctx);
    const float* left2 = block2.channel(0);

    // Must start generating sound from sample 0 of quantum 2 (offset 0)
    // Sine starts at phase 0, so sample 1, 2, ... will be non-zero
    bool has_sound = false;
    for (uint32_t i = 1; i < quantum; ++i) {
        if (std::abs(left2[i]) > 1e-6f) {
            has_sound = true;
        }
    }
    assert(has_sound);

    std::cout << "  [PASS] Boundary accuracy: event at sample 128 excluded from quantum 1 and active in quantum 2\n";
}

void test_c_two_events_same_sample() {
    std::cout << "[RUN] test_c_two_events_same_sample\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // Schedule two NoteOn events at the exact same sample (offset 20) with different pitches & IDs
    assert(engine.schedule_note_on(20, /*id=*/1, /*pitch=*/60.0, /*vel=*/1.0f)); // C4
    assert(engine.schedule_note_on(20, /*id=*/2, /*pitch=*/67.0, /*vel=*/1.0f)); // G4

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    const float* left = block.channel(0);

    // Samples [0, 20) must be silent
    for (uint32_t i = 0; i < 20; ++i) {
        assert(left[i] == 0.0f);
    }

    // From sample 20 onward, both voices sum together
    bool active = false;
    for (uint32_t i = 20; i < quantum; ++i) {
        if (std::abs(left[i]) > 1e-6f) active = true;
    }
    assert(active);

    std::cout << "  [PASS] Two NoteOn events at identical sample processed cleanly and simultaneously\n";
}

void test_d_same_pitch_different_note_id() {
    std::cout << "[RUN] test_d_same_pitch_different_note_id\n";

    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();

    // Both notes have pitch C4 (60.0), but different NoteId
    assert(engine.schedule_note_on(0, /*id=*/101, /*pitch=*/60.0, /*vel=*/0.5f));
    assert(engine.schedule_note_on(0, /*id=*/102, /*pitch=*/60.0, /*vel=*/0.5f));

    // NoteOff only for id 101 at sample 64
    assert(engine.schedule_note_off(64, /*id=*/101));

    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    const float* left = block.channel(0);

    // From 0 to 64: two voices playing
    // From 64 to 128: one voice (id 102) STILL playing! Must not be silent.
    bool second_half_active = false;
    for (uint32_t i = 65; i < 128; ++i) {
        if (std::abs(left[i]) > 1e-6f) {
            second_half_active = true;
        }
    }
    assert(second_half_active);

    std::cout << "  [PASS] Same pitch with different NoteId: NoteOff(101) does not stop NoteId 102\n";
}

void test_g_event_queue_invariants() {
    std::cout << "[RUN] test_g_event_queue_invariants\n";

    events::EventQueue queue;

    // 1. Monotonic ordering: out-of-order rejected
    assert(queue.schedule_note_on(100, 1, 60.0, 1.0f));
    assert(queue.schedule_note_on(200, 2, 64.0, 1.0f));
    assert(!queue.schedule_note_on(150, 3, 67.0, 1.0f)); // Out-of-order! Must be rejected
    assert(queue.rejected_out_of_order_count() == 1);

    // 2. Capacity overflow: reject when ring buffer is full
    events::EventQueue small_queue;
    for (size_t i = 0; i < events::EventQueue::kCapacity; ++i) {
        const bool ok = small_queue.schedule_note_on(static_cast<time::SamplePosition>(i), i + 1, 60.0, 1.0f);
        assert(ok);
    }
    // Next schedule must overflow and be rejected
    const bool overflow_ok = small_queue.schedule_note_on(999999, 999, 60.0, 1.0f);
    assert(!overflow_ok);
    assert(small_queue.dropped_overflow_count() == 1);

    // 3. Stale events after seek discarded
    audio::AudioEngine engine(make_synth_plan());
    const uint32_t quantum = 128;
    audio::AudioBuffer buffer(2, quantum);
    audio::ProcessContext ctx{48000.0, quantum};

    engine.play();
    engine.schedule_note_on(50, 1, 60.0, 1.0f); // Scheduled for sample 50

    // Seek past sample 50 to sample 100 before rendering
    engine.seek_samples(100);

    // Process quantum [100, 228)
    auto block = buffer.block(quantum);
    engine.process(block, ctx);

    // The event at sample 50 was stale and must have been safely discarded without playing
    const float* left = block.channel(0);
    for (uint32_t i = 0; i < quantum; ++i) {
        assert(left[i] == 0.0f);
    }

    std::cout << "  [PASS] EventQueue invariants: monotonic check, overflow rejection, stale discard on seek\n";
}

int main() {
    try {
        test_a_event_offset_accuracy();
        test_b_quantum_boundaries();
        test_c_two_events_same_sample();
        test_d_same_pitch_different_note_id();
        test_g_event_queue_invariants();
        std::cout << "All Event Model tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
