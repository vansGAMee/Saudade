#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

namespace {

std::shared_ptr<saudade::renderplan::RenderPlan> make_plan(float freq) {
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

void test_single_publication_and_observation() {
    std::cout << "[RUN] test_single_publication_and_observation\n";

    auto plan1 = make_plan(440.0f);
    saudade::audio::AudioEngine engine(plan1);

    assert(engine.publisher().active_generation() == 1);
    assert(engine.publisher().completed_generation() == 0);

    const uint32_t quantum = 256;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    // 1. RT executes quantum on initial plan
    auto block = buffer.block(quantum);
    engine.process(block, ctx);
    assert(engine.publisher().completed_generation() == 1);

    // 2. Control publishes Generation 2
    auto plan2 = make_plan(660.0f);
    const auto gen2 = engine.publish_plan(plan2);
    assert(gen2 == 2);
    assert(engine.publisher().active_generation() == 2);
    assert(engine.publisher().retired_count() == 1);

    // Before RT executes Gen 2, Gen 1 cannot be reclaimed
    size_t reclaimed = engine.collect_retired();
    assert(reclaimed == 0);
    assert(engine.publisher().retired_count() == 1);

    // 3. RT executes quantum on Gen 2
    engine.process(block, ctx);
    assert(engine.publisher().completed_generation() == 2);

    // Now Gen 1 can be reclaimed
    reclaimed = engine.collect_retired();
    assert(reclaimed == 1);
    assert(engine.publisher().retired_count() == 0);

    std::cout << "  [PASS] Single publication, observation, and reclamation verified\n";
}

void test_rapid_publication() {
    std::cout << "[RUN] test_rapid_publication\n";

    auto initial_plan = make_plan(440.0f);
    saudade::audio::AudioEngine engine(initial_plan);

    const uint32_t quantum = 128;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    std::atomic<bool> stop_rt{false};
    std::atomic<uint64_t> rt_quanta{0};

    // Synthetic RT thread running at ~1 kHz
    std::thread rt_thread([&]() {
        while (!stop_rt.load(std::memory_order_relaxed)) {
            auto block = buffer.block(quantum);
            engine.process(block, ctx);
            rt_quanta.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Control thread rapidly publishes 50 generations
    saudade::audio::PlanGeneration last_gen = 1;
    for (int i = 0; i < 50; ++i) {
        auto p = make_plan(440.0f + static_cast<float>(i) * 10.0f);
        last_gen = engine.publish_plan(p);
        engine.collect_retired();
    }

    // Wait until RT thread observes the latest generation
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (engine.publisher().completed_generation() < last_gen &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        engine.collect_retired();
    }

    assert(engine.publisher().completed_generation() == last_gen);

    // Stop RT thread
    stop_rt.store(true, std::memory_order_relaxed);
    rt_thread.join();

    // After RT finishes, all retired plans must be reclaimable
    while (engine.publisher().retired_count() > 0) {
        engine.collect_retired();
    }
    assert(engine.publisher().retired_count() == 0);

    std::cout << "  [PASS] Rapid publication: 50 generations without UAF, latest observed ("
              << last_gen << "), all retired reclaimed\n";
}

void test_shutdown_reclamation() {
    std::cout << "[RUN] test_shutdown_reclamation\n";

    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_gen1;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_gen2;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_gen3;

    {
        auto p1 = make_plan(220.0f);
        weak_gen1 = p1;
        saudade::audio::AudioEngine engine(p1);

        auto p2 = make_plan(440.0f);
        weak_gen2 = p2;
        engine.publish_plan(p2);

        auto p3 = make_plan(880.0f);
        weak_gen3 = p3;
        engine.publish_plan(p3);

        // Before engine destruction, weak pointers should be alive
        assert(!weak_gen1.expired());
        assert(!weak_gen2.expired());
        assert(!weak_gen3.expired());
    }

    // After engine destruction, all plans must be destroyed
    assert(weak_gen1.expired());
    assert(weak_gen2.expired());
    assert(weak_gen3.expired());

    std::cout << "  [PASS] Shutdown: Active and retired plans destroyed without memory leak\n";
}

int main() {
    try {
        test_single_publication_and_observation();
        test_rapid_publication();
        test_shutdown_reclamation();
        std::cout << "All plan publication tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
