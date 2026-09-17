#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>
#include <saudade/time/time_types.hpp>
#include <saudade/time/transport.hpp>
#include <saudade/events/event_types.hpp>

#include <iostream>
#include <iomanip>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>
#include <cmath>

namespace {
std::atomic<bool> g_stop{false};

extern "C" void signal_handler(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

std::unique_ptr<saudade::renderplan::RenderPlan> create_synth_plan() {
    saudade::graph::GraphModel graph;
    const auto synth_node = graph.add_poly_synth_node();
    const auto gain_node = graph.add_gain_node(-12.0f);
    const auto out_node = graph.add_output_node(2);

    graph.connect(synth_node, saudade::graph::PolySynthNode::kPortOut,
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
        // 2. Compile RenderPlan: PolySynth -> Gain(-12 dB) -> Stereo Output
        auto plan = create_synth_plan();

        // 3. Initialize AudioEngine with compiled plan
        saudade::audio::AudioEngine engine(std::move(plan));

        // 4. Connect PipeWire boundary endpoint
        saudade::pipewire::PipeWireEndpoint endpoint(engine, "saudade-audio-proof");
        endpoint.start();

        // Wait for PipeWire stream negotiation
        if (!endpoint.wait_for_stream(2000)) {
            std::cerr << "Warning: Waiting for audio device stream connection...\n";
        }

        const double sr = endpoint.sample_rate() > 0 ? endpoint.sample_rate() : 48000.0;

        // 5. Schedule melody at 120 BPM via TempoMap
        auto to_sample = [&](double beat) -> saudade::time::SamplePosition {
            const int64_t ticks = static_cast<int64_t>(std::llround(beat * static_cast<double>(saudade::time::BeatPosition::kTicksPerBeat)));
            return engine.transport().tempo_map().beat_to_sample(saudade::time::BeatPosition::from_ticks(ticks), sr);
        };

        // Note 1: C4 (pitch 60.0) [Beat 0.0 -> 0.8]
        engine.schedule_note_on(to_sample(0.0), /*id=*/1, /*pitch=*/60.0, /*vel=*/0.8f);
        engine.schedule_note_off(to_sample(0.8), /*id=*/1);

        // Note 2: E4 (pitch 64.0) [Beat 1.0 -> 1.8]
        engine.schedule_note_on(to_sample(1.0), /*id=*/2, /*pitch=*/64.0, /*vel=*/0.8f);
        engine.schedule_note_off(to_sample(1.8), /*id=*/2);

        // Note 3: G4 (pitch 67.0) [Beat 2.0 -> 2.8]
        engine.schedule_note_on(to_sample(2.0), /*id=*/3, /*pitch=*/67.0, /*vel=*/0.8f);
        engine.schedule_note_off(to_sample(2.8), /*id=*/3);

        // Chord: C4 + E4 + G4 simultaneous [Beat 3.0 -> 3.8]
        engine.schedule_note_on(to_sample(3.0), /*id=*/4, /*pitch=*/60.0, /*vel=*/0.7f);
        engine.schedule_note_on(to_sample(3.0), /*id=*/5, /*pitch=*/64.0, /*vel=*/0.7f);
        engine.schedule_note_on(to_sample(3.0), /*id=*/6, /*pitch=*/67.0, /*vel=*/0.7f);
        engine.schedule_note_off(to_sample(3.8), /*id=*/4);
        engine.schedule_note_off(to_sample(3.8), /*id=*/5);
        engine.schedule_note_off(to_sample(3.8), /*id=*/6);

        // 6. Print banner and scheduled melody details
        std::cout << "==================================================\n"
                  << "Saudade Audio Proof -- Milestone 4: PolySynth & Events\n"
                  << "==================================================\n"
                  << "Backend: PipeWire\n"
                  << "Graph: PolySynth(8 voices) -> Gain(-12 dB) -> Stereo Output\n"
                  << "Sample rate: " << endpoint.sample_rate() << " Hz\n"
                  << "Quantum: " << endpoint.quantum() << " frames\n"
                  << "Tempo: 120.0 BPM (1 beat = 0.5s = " << to_sample(1.0) << " samples)\n"
                  << "Scheduled Melody (Control-side log):\n"
                  << "  Beat 0.00 (sample " << std::setw(6) << to_sample(0.0) << "): NoteOn  C4 (pitch 60.0) [ID 1]\n"
                  << "  Beat 0.80 (sample " << std::setw(6) << to_sample(0.8) << "): NoteOff C4 [ID 1]\n"
                  << "  Beat 1.00 (sample " << std::setw(6) << to_sample(1.0) << "): NoteOn  E4 (pitch 64.0) [ID 2]\n"
                  << "  Beat 1.80 (sample " << std::setw(6) << to_sample(1.8) << "): NoteOff E4 [ID 2]\n"
                  << "  Beat 2.00 (sample " << std::setw(6) << to_sample(2.0) << "): NoteOn  G4 (pitch 67.0) [ID 3]\n"
                  << "  Beat 2.80 (sample " << std::setw(6) << to_sample(2.8) << "): NoteOff G4 [ID 3]\n"
                  << "  Beat 3.00 (sample " << std::setw(6) << to_sample(3.0) << "): NoteOn  C4+E4+G4 triad chord [IDs 4, 5, 6]\n"
                  << "  Beat 3.80 (sample " << std::setw(6) << to_sample(3.8) << "): NoteOff C4+E4+G4 chord [IDs 4, 5, 6]\n"
                  << "==================================================\n"
                  << "Starting transport playback...\n"
                  << std::flush;

        engine.play();

        const auto start_time = std::chrono::steady_clock::now();
        auto last_print_time = start_time;

        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

            // Periodic progress reporting every 300ms
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print_time).count() >= 300) {
                last_print_time = now;
                const auto pos = engine.transport().current_sample();
                const auto beat = engine.transport().current_beat(sr);
                const char* state_str = engine.transport().is_playing() ? "PLAYING" : "STOPPED";
                std::cout << "  [Playback] State: " << std::left << std::setw(8) << state_str
                          << " | Sample: " << std::setw(7) << pos
                          << " | Beat: " << std::fixed << std::setprecision(2) << beat.to_double()
                          << "\n" << std::flush;
            }

            // Melody is 4 beats (2.0s). Let it play to ~3.0s (6 beats) then stop cleanly.
            if (elapsed >= 3000) {
                std::cout << ">>> Melody playback complete. Stopping transport.\n" << std::flush;
                engine.stop();
                break;
            }

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
