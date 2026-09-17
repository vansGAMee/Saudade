#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/allocation_guard.hpp>

#include <cassert>
#include <iostream>
#include <vector>

namespace {

std::shared_ptr<saudade::renderplan::RenderPlan> make_sine_plan(float freq) {
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

void test_zero_realtime_allocations() {
    std::cout << "[RUN] test_zero_realtime_allocations\n";

    auto plan = make_sine_plan(440.0f);
    saudade::audio::AudioEngine engine(plan);

    const uint32_t quantum = 512;
    saudade::audio::AudioBuffer out_buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    saudade::audio::reset_realtime_allocation_count();

    // Execute 100 blocks inside realtime guard
    for (int i = 0; i < 100; ++i) {
        saudade::audio::AudioBlock block = out_buffer.block(quantum);
        engine.process(block, ctx);
    }

    const size_t violations = saudade::audio::get_realtime_allocation_count();
    assert(violations == 0);
    std::cout << "  [PASS] Zero allocations detected during 100 realtime process blocks\n";
}

void test_zero_allocations_during_plan_swaps() {
    std::cout << "[RUN] test_zero_allocations_during_plan_swaps\n";

    auto plan1 = make_sine_plan(440.0f);
    auto plan2 = make_sine_plan(660.0f);
    auto plan3 = make_sine_plan(880.0f);

    saudade::audio::AudioEngine engine(plan1);

    const uint32_t quantum = 256;
    saudade::audio::AudioBuffer out_buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    saudade::audio::reset_realtime_allocation_count();

    // 1. Process block on initial plan
    auto block = out_buffer.block(quantum);
    engine.process(block, ctx);
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    // 2. Publish plan 2 (allocation outside RT guard is normal)
    engine.publish_plan(plan2);

    // 3. Process block immediately following publication
    engine.process(block, ctx);
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    // 4. Publish plan 3
    engine.publish_plan(plan3);

    // 5. Process block on plan 3
    engine.process(block, ctx);
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    // Reclaim retired outside RT guard
    engine.collect_retired();

    std::cout << "  [PASS] Zero allocations detected in realtime process() during plan swaps\n";
}

void test_allocation_guard_catches_violations() {
    std::cout << "[RUN] test_allocation_guard_catches_violations\n";

    saudade::audio::reset_realtime_allocation_count();
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    // Dynamic allocation outside guard should NOT be counted as violation
    {
        volatile int* p = new int(123);
        delete p;
    }
    assert(saudade::audio::get_realtime_allocation_count() == 0);

    // Dynamic allocation inside guard MUST be detected
    {
        saudade::audio::ScopedRealtimeGuard guard;
        assert(saudade::audio::is_realtime_scope_active());

        volatile int* p = new int(456);
        delete p;
    }
    assert(!saudade::audio::is_realtime_scope_active());
    assert(saudade::audio::get_realtime_allocation_count() > 0);
    std::cout << "  [PASS] ScopedRealtimeGuard successfully intercepted heap allocation\n";

    saudade::audio::reset_realtime_allocation_count();
    assert(saudade::audio::get_realtime_allocation_count() == 0);
}

int main() {
    try {
        test_zero_realtime_allocations();
        test_zero_allocations_during_plan_swaps();
        test_allocation_guard_catches_violations();
        std::cout << "All allocation guard tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Allocation guard test failed: " << ex.what() << "\n";
        return 1;
    }
}
