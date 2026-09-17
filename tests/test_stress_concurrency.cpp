#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/allocation_guard.hpp>

#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <memory>

namespace {

std::unique_ptr<saudade::renderplan::RenderPlan> make_plan(float freq) {
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(freq);
    const auto gain = graph.add_gain_node(-12.0f);
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

void test_high_concurrency_stress() {
    std::cout << "[RUN] test_high_concurrency_stress\n";

    saudade::audio::AudioEngine engine(make_plan(220.0f));

    constexpr uint32_t quantum = 128;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    std::atomic<bool> publisher_done{false};
    std::atomic<uint64_t> rt_quanta_count{0};
    constexpr int kTotalPublications = 1000;

    saudade::audio::reset_realtime_allocation_count();

    // 1. Synthetic RT thread running at high frequency
    std::thread rt_thread([&]() {
        while (!publisher_done.load(std::memory_order_relaxed)) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            rt_quanta_count.fetch_add(1, std::memory_order_relaxed);
        }
        // Drain a few extra blocks to acknowledge final generation
        for (int i = 0; i < 100; ++i) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            rt_quanta_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // 2. Control thread publishing rapidly
    saudade::audio::PlanGeneration last_published = 1;
    for (int i = 1; i <= kTotalPublications; ++i) {
        last_published = engine.publish_plan(make_plan(220.0f + static_cast<float>(i % 8) * 110.0f));

        // Periodically reclaim retired plans
        if (i % 10 == 0) {
            engine.collect_retired();
        }
    }

    assert(last_published == 1 + kTotalPublications);

    // Wait until RT acknowledges the latest generation
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (engine.publisher().completed_generation() < last_published &&
           std::chrono::steady_clock::now() < deadline) {
        engine.collect_retired();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    publisher_done.store(true, std::memory_order_relaxed);
    rt_thread.join();

    // Final drain of all remaining retired plans
    while (engine.publisher().retired_count() > 0) {
        engine.collect_retired();
    }

    // Verify all stress properties
    assert(engine.publisher().completed_generation() >= last_published);
    assert(engine.publisher().retired_count() == 0);
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    std::cout << "  [PASS] Stress test completed:\n"
              << "    - Total publications: " << kTotalPublications << "\n"
              << "    - RT quanta processed: " << rt_quanta_count.load() << "\n"
              << "    - Final generation: " << engine.publisher().completed_generation() << "\n"
              << "    - Remaining retired plans: " << engine.publisher().retired_count() << "\n"
              << "    - RT memory allocation violations: "
              << saudade::audio::get_realtime_allocation_count() << "\n";
}

int main() {
    try {
        test_high_concurrency_stress();
        std::cout << "All stress concurrency tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
