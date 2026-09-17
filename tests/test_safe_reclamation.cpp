#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <cassert>
#include <iostream>
#include <memory>

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

void test_safe_reclamation_lifecycle() {
    std::cout << "[RUN] test_safe_reclamation_lifecycle\n";

    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_plan1;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_plan2;

    auto plan1 = make_plan(440.0f);
    weak_plan1 = plan1;
    saudade::audio::AudioEngine engine(std::move(plan1));

    assert(!weak_plan1.expired());

    const uint32_t quantum = 256;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};

    // 1. RT executes quantum on initial plan (Gen 1)
    auto block = buffer.block(quantum);
    engine.process(block, ctx);
    assert(engine.publisher().completed_generation() == 1);

    // 2. Control publishes Generation 2
    auto plan2 = make_plan(660.0f);
    weak_plan2 = plan2;
    engine.publish_plan(std::move(plan2));

    // Both plans must still be alive!
    assert(!weak_plan1.expired());
    assert(!weak_plan2.expired());
    assert(engine.publisher().retired_count() == 1);

    // 3. Attempt reclamation BEFORE RT acknowledges Gen 2.
    // Plan 1 MUST NOT be destroyed!
    const size_t reclaimed_before = engine.collect_retired();
    assert(reclaimed_before == 0);
    assert(!weak_plan1.expired());
    assert(engine.publisher().retired_count() == 1);

    // 4. RT executes quantum on Gen 2
    engine.process(block, ctx);
    assert(engine.publisher().completed_generation() == 2);

    // Still alive until control calls collect_retired()
    assert(!weak_plan1.expired());

    // 5. Control reclaims retired plans now that RT is at generation 2
    const size_t reclaimed_after = engine.collect_retired();
    assert(reclaimed_after == 1);
    assert(weak_plan1.expired());   // Plan 1 is now destroyed!
    assert(!weak_plan2.expired());  // Plan 2 is active and alive!
    assert(engine.publisher().retired_count() == 0);

    std::cout << "  [PASS] Plan 1 preserved during RT execution and destroyed only after acknowledgement\n";
}

void test_skipped_generations_reclamation() {
    std::cout << "[RUN] test_skipped_generations_reclamation\n";

    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_g1;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_g2;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_g3;
    std::weak_ptr<const saudade::renderplan::RenderPlan> weak_g4;

    auto p1 = make_plan(100.0f);
    weak_g1 = p1;
    saudade::audio::AudioEngine engine(std::move(p1));

    // Publish Gen 2, Gen 3, Gen 4 rapidly before RT runs Gen 2 or 3
    auto p2 = make_plan(200.0f);
    weak_g2 = p2;
    engine.publish_plan(std::move(p2));

    auto p3 = make_plan(300.0f);
    weak_g3 = p3;
    engine.publish_plan(std::move(p3));

    auto p4 = make_plan(400.0f);
    weak_g4 = p4;
    engine.publish_plan(std::move(p4));

    // All plans must be alive
    assert(!weak_g1.expired());
    assert(!weak_g2.expired());
    assert(!weak_g3.expired());
    assert(!weak_g4.expired());
    assert(engine.publisher().retired_count() == 3);

    // Reclamation while RT completed is 0 must reclaim nothing
    assert(engine.collect_retired() == 0);
    assert(engine.publisher().retired_count() == 3);

    // RT runs one quantum: directly picks up Gen 4 (skipping Gen 2 & Gen 3!)
    const uint32_t quantum = 256;
    saudade::audio::AudioBuffer buffer(2, quantum);
    saudade::audio::ProcessContext ctx{48000.0, quantum};
    auto block = buffer.block(quantum);

    engine.process(block, ctx);
    assert(engine.publisher().completed_generation() == 4);

    // Now collect_retired() must reclaim Gen 1, Gen 2, and Gen 3
    const size_t reclaimed = engine.collect_retired();
    assert(reclaimed == 3);
    assert(weak_g1.expired());
    assert(weak_g2.expired());
    assert(weak_g3.expired());
    assert(!weak_g4.expired()); // Gen 4 is still active
    assert(engine.publisher().retired_count() == 0);

    std::cout << "  [PASS] Skipped generations safely reclaimed without memory leaks\n";
}

int main() {
    try {
        test_safe_reclamation_lifecycle();
        test_skipped_generations_reclamation();
        std::cout << "All safe reclamation tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
