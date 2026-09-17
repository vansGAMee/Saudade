#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>

#include <cassert>
#include <iostream>

void test_valid_graph_compilation() {
    std::cout << "[RUN] test_valid_graph_compilation\n";
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(440.0f);
    const auto gain = graph.add_gain_node(-12.0f);
    const auto out = graph.add_output_node(2);

    assert(graph.connect(sine, saudade::graph::SineNode::kPortOut,
                         gain, saudade::graph::GainNode::kPortIn));
    assert(graph.connect(gain, saudade::graph::GainNode::kPortOut,
                         out, saudade::graph::OutputNode::kPortLeft));
    assert(graph.connect(gain, saudade::graph::GainNode::kPortOut,
                         out, saudade::graph::OutputNode::kPortRight));

    auto plan = saudade::graph::GraphCompiler::compile(graph);
    assert(plan != nullptr);
    assert(plan->num_scratch_buffers() >= 2);
    assert(plan->num_sine_states() == 1);
    assert(plan->output_channels() == 2);
    assert(plan->steps().size() == 4); // 1 sine + 1 gain + 2 routes
    std::cout << "  [PASS] Valid graph compiles into immutable RenderPlan\n";
}

void test_invalid_empty_graph() {
    std::cout << "[RUN] test_invalid_empty_graph\n";
    saudade::graph::GraphModel graph;
    bool caught = false;
    try {
        auto plan = saudade::graph::GraphCompiler::compile(graph);
        (void)plan;
    } catch (const saudade::graph::GraphCompilationException&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  [PASS] Empty graph correctly rejected\n";
}

void test_missing_output_node() {
    std::cout << "[RUN] test_missing_output_node\n";
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(440.0f);
    const auto gain = graph.add_gain_node(-12.0f);
    graph.connect(sine, "out", gain, "in");

    bool caught = false;
    try {
        auto plan = saudade::graph::GraphCompiler::compile(graph);
        (void)plan;
    } catch (const saudade::graph::GraphCompilationException&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  [PASS] Missing OutputNode correctly rejected\n";
}

void test_disconnected_ports() {
    std::cout << "[RUN] test_disconnected_ports\n";
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(440.0f);
    const auto gain = graph.add_gain_node(-12.0f);
    graph.add_output_node(2);

    // Sine connected to gain, but out left/right not connected
    graph.connect(sine, "out", gain, "in");

    bool caught = false;
    try {
        auto plan = saudade::graph::GraphCompiler::compile(graph);
        (void)plan;
    } catch (const saudade::graph::GraphCompilationException&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  [PASS] Unconnected Output ports correctly rejected\n";
}

void test_cycle_detection() {
    std::cout << "[RUN] test_cycle_detection\n";
    saudade::graph::GraphModel graph;
    const auto gain1 = graph.add_gain_node(-6.0f);
    const auto gain2 = graph.add_gain_node(-6.0f);
    const auto out = graph.add_output_node(2);

    // Create a cycle: gain1 -> gain2 -> gain1
    // Note: port names must be valid to pass port validation and hit cycle detection
    graph.connect(gain1, saudade::graph::GainNode::kPortOut,
                  gain2, saudade::graph::GainNode::kPortIn);
    graph.connect(gain2, saudade::graph::GainNode::kPortOut,
                  gain1, saudade::graph::GainNode::kPortIn);
    graph.connect(gain1, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortLeft);
    graph.connect(gain1, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortRight);

    bool caught = false;
    try {
        auto plan = saudade::graph::GraphCompiler::compile(graph);
        (void)plan;
    } catch (const saudade::graph::GraphCompilationException&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  [PASS] Cycles in graph correctly rejected\n";
}

int main() {
    try {
        test_valid_graph_compilation();
        test_invalid_empty_graph();
        test_missing_output_node();
        test_disconnected_ports();
        test_cycle_detection();
        std::cout << "All graph compiler tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Graph compiler test failed: " << ex.what() << "\n";
        return 1;
    }
}
