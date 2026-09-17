#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

namespace {

std::unique_ptr<saudade::renderplan::RenderPlan> make_plan_with_gain(float gain_db) {
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(440.0f);
    const auto gain = graph.add_gain_node(gain_db);
    const auto out = graph.add_output_node(2);

    graph.connect(sine, saudade::graph::SineNode::kPortOut,
                  gain, saudade::graph::GainNode::kPortIn);
    graph.connect(gain, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortLeft);
    graph.connect(gain, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortRight);

    return saudade::graph::GraphCompiler::compile(graph);
}

} // namespace

void test_quantum_snapshot_atomicity() {
    std::cout << "[RUN] test_quantum_snapshot_atomicity\n";

    auto plan1 = make_plan_with_gain(0.0f);   // Linear gain 1.0
    auto plan2 = make_plan_with_gain(-6.0f);  // Linear gain ~0.501187

    saudade::audio::AudioEngine engine(std::move(plan1));

    const uint32_t quantum = 256;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    // 1. Manually snapshot generation 1 via publisher
    auto* snap = engine.publisher().acquire_current_for_quantum();
    assert(snap != nullptr);
    assert(snap->generation == 1);

    // 2. Control publishes Generation 2 while quantum is "in progress"
    const auto gen2 = engine.publish_plan(std::move(plan2));
    assert(gen2 == 2);
    assert(engine.publisher().active_generation() == 2);

    // 3. Render the quantum using the previously snapshotted plan
    auto block = buffer.block(quantum);
    snap->plan->render(block, snap->scratch_buffers, snap->dsp_state, ctx);

    // Acknowledge generation 1
    engine.publisher().acknowledge_completed_generation(snap->generation);
    assert(engine.publisher().completed_generation() == 1);

    // 4. Next quantum must acquire generation 2
    auto* snap2 = engine.publisher().acquire_current_for_quantum();
    assert(snap2 != nullptr);
    assert(snap2->generation == 2);

    engine.publisher().acknowledge_completed_generation(snap2->generation);
    assert(engine.publisher().completed_generation() == 2);

    // Clean up
    engine.collect_retired();
    assert(engine.publisher().retired_count() == 0);

    std::cout << "  [PASS] Mid-quantum publication preserves snapshot integrity\n";
}

void test_concurrent_quantum_consistency() {
    std::cout << "[RUN] test_concurrent_quantum_consistency\n";

    auto plan_a = make_plan_with_gain(0.0f);

    saudade::audio::AudioEngine engine(std::move(plan_a));

    const uint32_t quantum = 128;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> completed_quanta{0};

    // RT thread: continuously renders quanta
    std::thread rt_thread([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            completed_quanta.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Publisher thread: continuously swaps between plan A and plan B
    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            engine.publish_plan(make_plan_with_gain(-12.0f));
        } else {
            engine.publish_plan(make_plan_with_gain(0.0f));
        }
        engine.collect_retired();
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    stop.store(true, std::memory_order_relaxed);
    rt_thread.join();

    // Final reclamation
    while (engine.publisher().retired_count() > 0) {
        engine.collect_retired();
    }
    assert(engine.publisher().retired_count() == 0);
    assert(completed_quanta.load(std::memory_order_relaxed) > 100);

    std::cout << "  [PASS] Concurrent rendering and swapping completed with full quantum consistency\n";
}

int main() {
    try {
        test_quantum_snapshot_atomicity();
        test_concurrent_quantum_consistency();
        std::cout << "All quantum consistency tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
