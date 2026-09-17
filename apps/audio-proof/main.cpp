#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>
#include <saudade/time/time_types.hpp>
#include <saudade/time/transport.hpp>

#include <iostream>
#include <iomanip>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>

namespace {
std::atomic<bool> g_stop{false};

extern "C" void signal_handler(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

std::unique_ptr<saudade::renderplan::RenderPlan> create_sine_plan(float frequency_hz, float gain_db) {
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
        // 2. Compile RenderPlan: Sine 440 Hz -> Gain -12 dB -> Stereo
        auto plan = create_sine_plan(440.0f, -12.0f);

        // 3. Initialize AudioEngine with initial plan
        saudade::audio::AudioEngine engine(std::move(plan));

        // 4. Connect PipeWire boundary endpoint
        saudade::pipewire::PipeWireEndpoint endpoint(engine, "saudade-audio-proof");
        endpoint.start();

        // Wait for PipeWire stream negotiation
        if (!endpoint.wait_for_stream(2000)) {
            std::cerr << "Warning: Waiting for audio device stream connection...\n";
        }

        const double sr = endpoint.sample_rate() > 0 ? endpoint.sample_rate() : 48000.0;

        // 5. Print initial banner
        std::cout << "==================================================\n"
                  << "Saudade Audio Proof -- Milestone 3: Time & Transport\n"
                  << "==================================================\n"
                  << "Backend: PipeWire\n"
                  << "Sample rate: " << endpoint.sample_rate() << " Hz\n"
                  << "Quantum: " << endpoint.quantum() << " frames\n"
                  << "Tempo: 120.0 BPM (1 beat = 0.5s = " << static_cast<int64_t>(0.5 * sr) << " samples)\n"
                  << "Initial state: Stopped at sample 0 (silence)\n"
                  << "Press Ctrl+C to stop.\n"
                  << "==================================================\n"
                  << std::flush;

        enum class DemoStep {
            InitialSilence,
            PlayingFirst,
            StoppedMid,
            SeekBeat4,
            PlayingSecond,
            Done
        };

        DemoStep step = DemoStep::InitialSilence;
        const auto start_time = std::chrono::steady_clock::now();
        auto last_print_time = start_time;

        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

            // Periodic progress reporting every 500ms
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print_time).count() >= 500) {
                last_print_time = now;
                const auto pos = engine.transport().current_sample();
                const auto beat = engine.transport().current_beat(sr);
                const char* state_str = engine.transport().is_playing() ? "PLAYING" : "STOPPED";
                std::cout << "  [Transport] State: " << std::left << std::setw(8) << state_str
                          << " | Pos: " << std::setw(8) << pos << " samples"
                          << " | Beat: " << std::fixed << std::setprecision(2) << beat.to_double()
                          << "\n" << std::flush;
            }

            // Step 1: At ~0.8s, command Play()
            if (step == DemoStep::InitialSilence && elapsed >= 800) {
                step = DemoStep::PlayingFirst;
                std::cout << ">>> [COMMAND] Play() -> 440 Hz tone starts, position advancing\n" << std::flush;
                engine.play();
            }
            // Step 2: At ~2.8s, command Stop() -> position freezes, silence
            else if (step == DemoStep::PlayingFirst && elapsed >= 2800) {
                step = DemoStep::StoppedMid;
                engine.stop();
                const auto freeze_pos = engine.transport().current_sample();
                const auto freeze_beat = engine.transport().current_beat(sr);
                std::cout << ">>> [COMMAND] Stop() -> Audio silenced, position frozen at "
                          << freeze_pos << " samples (" << std::fixed << std::setprecision(2)
                          << freeze_beat.to_double() << " beats)\n" << std::flush;
            }
            // Step 3: At ~3.8s, command Seek to Beat 4
            else if (step == DemoStep::StoppedMid && elapsed >= 3800) {
                step = DemoStep::SeekBeat4;
                const auto target_beat = saudade::time::BeatPosition::from_beats(4);
                engine.seek_beats(target_beat, sr);
                const auto target_sample = engine.transport().current_sample();
                std::cout << ">>> [COMMAND] Seek to Beat 4.0 -> Sample position updated to "
                          << target_sample << "\n" << std::flush;
            }
            // Step 4: At ~4.3s, command Play() -> resumes from Beat 4
            else if (step == DemoStep::SeekBeat4 && elapsed >= 4300) {
                step = DemoStep::PlayingSecond;
                std::cout << ">>> [COMMAND] Play() -> Resuming playback from Beat 4.0\n" << std::flush;
                engine.play();
            }
            // Step 5: At ~6.5s, demonstration complete
            else if (step == DemoStep::PlayingSecond && elapsed >= 6500) {
                step = DemoStep::Done;
                std::cout << ">>> Demonstration complete. Stopping transport.\n" << std::flush;
                engine.stop();
                break;
            }

            // Periodically collect retired plans
            engine.collect_retired();
        }

        std::cout << "\nStopping PipeWire endpoint...\n";
        endpoint.stop();
        engine.collect_retired();
        std::cout << "Audio endpoint cleanly stopped. Shutdown complete.\n";

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
