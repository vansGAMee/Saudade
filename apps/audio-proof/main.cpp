#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>

#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>

namespace {
std::atomic<bool> g_stop{false};

extern "C" void signal_handler(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

std::shared_ptr<saudade::renderplan::RenderPlan> create_sine_plan(float frequency_hz, float gain_db) {
    saudade::graph::GraphModel graph;
    const auto sine_node = graph.add_sine_node(frequency_hz);
    const auto gain_node = graph.add_gain_node(gain_db);
    const auto out_node = graph.add_output_node(2);

    graph.connect(sine_node, saudade::graph::SineNode::kPortOut,
                  gain_node, saudade::graph::GainNode::kPortIn);
    graph.connect(gain_node, saudade::graph::GainNode::kPortOut,
                  out_node, saudade::graph::OutputNode::kPortLeft);
    graph.connect(gain_node, saudade::graph::GainNode::kPortOut,
                  out_node, saudade::graph::OutputNode::kPortRight);

    return saudade::graph::GraphCompiler::compile(graph);
}
} // namespace

int main() {
    // 1. Install async-signal-safe signal handlers
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    try {
        // 2. Compile Generation 1: 440 Hz
        auto plan_gen1 = create_sine_plan(440.0f, -12.0f);

        // 3. Initialize AudioEngine with Generation 1
        saudade::audio::AudioEngine engine(plan_gen1);

        // 4. Connect PipeWire boundary endpoint
        saudade::pipewire::PipeWireEndpoint endpoint(engine, "saudade-audio-proof");
        endpoint.start();

        // Wait for PipeWire stream negotiation
        if (!endpoint.wait_for_stream(2000)) {
            std::cerr << "Warning: Waiting for audio device stream connection...\n";
        }

        // 5. Print initial banner
        std::cout << "Audio backend: PipeWire\n"
                  << "Sample rate: " << endpoint.sample_rate() << "\n"
                  << "Quantum: " << endpoint.quantum() << "\n"
                  << "Generation 1 active: Sine(440 Hz) -> Gain(-12 dB) -> Output\n"
                  << "Playing. Generation updates will occur automatically.\n"
                  << "Press Ctrl+C to stop.\n"
                  << std::flush;

        // 6. Control thread loop for live plan swapping
        int step = 0;
        const auto start_time = std::chrono::steady_clock::now();

        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();

            // At ~2.0s: compile and publish Generation 2 (660 Hz) on control thread
            if (step == 0 && elapsed >= 2000) {
                step = 1;
                auto plan_gen2 = create_sine_plan(660.0f, -12.0f);
                const auto gen = engine.publish_plan(plan_gen2);
                std::cout << "Published generation " << gen << ": 660 Hz\n" << std::flush;
            }
            // At ~4.0s: compile and publish Generation 3 (440 Hz) on control thread
            else if (step == 1 && elapsed >= 4000) {
                step = 2;
                auto plan_gen3 = create_sine_plan(440.0f, -12.0f);
                const auto gen = engine.publish_plan(plan_gen3);
                std::cout << "Published generation " << gen << ": 440 Hz\n" << std::flush;
            }

            // Periodically collect retired plans on control thread
            engine.collect_retired();
        }

        std::cout << "\nStopping playback...\n";
        endpoint.stop();
        // Post-quiescence reclamation: collect any remaining retired plans
        engine.collect_retired();
        std::cout << "Playback cleanly stopped. Retired count: "
                  << engine.publisher().retired_count() << "\n";

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
