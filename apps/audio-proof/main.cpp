#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>
#include <saudade/time/time_types.hpp>
#include <saudade/time/transport.hpp>
#include <saudade/events/event_types.hpp>
#include <saudade/model/pattern.hpp>
#include <saudade/model/pattern_compiler.hpp>

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

        // 5. Build canonical musical Pattern (Milestone 5)
        saudade::model::Pattern pattern(1, "DemoMelody", saudade::time::BeatDuration::from_beats(4));
        const auto lane_id = pattern.add_lane("SynthLead");
        auto* lane = pattern.find_lane(lane_id);
        if (!lane) {
            throw std::runtime_error("Failed to create pattern lane");
        }

        auto& seq = lane->notes();

        // Note 1: C4 (pitch 60.0) [Beat 0.0 -> 0.8]
        seq.add_note(saudade::time::BeatPosition::zero(),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     60.0, 0.8f);

        // Note 2: E4 (pitch 64.0) [Beat 1.0 -> 1.8]
        seq.add_note(saudade::time::BeatPosition::from_beats(1),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     64.0, 0.8f);

        // Note 3: G4 (pitch 67.0) [Beat 2.0 -> 2.8]
        seq.add_note(saudade::time::BeatPosition::from_beats(2),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     67.0, 0.8f);

        // Chord: C4 + E4 + G4 simultaneous [Beat 3.0 -> 3.8]
        seq.add_note(saudade::time::BeatPosition::from_beats(3),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     60.0, 0.7f);
        seq.add_note(saudade::time::BeatPosition::from_beats(3),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     64.0, 0.7f);
        seq.add_note(saudade::time::BeatPosition::from_beats(3),
                     saudade::time::BeatDuration::from_fraction(8, 10),
                     67.0, 0.7f);

        // Compile Pattern control-side into sample-accurate TimelineEvents
        const auto compiled_events = saudade::model::PatternCompiler::compile(
            pattern,
            engine.transport().tempo_map(),
            sr
        );

        // Ingress compiled TimelineEvents into AudioEngine queue
        for (const auto& ev : compiled_events) {
            engine.schedule_event(ev);
        }

        // 6. Print banner and scheduled melody details
        std::cout << "==================================================\n"
                  << "Saudade Audio Proof -- Milestone 5: Canonical Pattern & Sequence Model\n"
                  << "==================================================\n"
                  << "Backend: PipeWire\n"
                  << "Graph: PolySynth(8 voices) -> Gain(-12 dB) -> Stereo Output\n"
                  << "Sample rate: " << endpoint.sample_rate() << " Hz\n"
                  << "Quantum: " << endpoint.quantum() << " frames\n"
                  << "Tempo: 120.0 BPM\n"
                  << "Pattern: '" << pattern.name() << "' (" << pattern.length().to_double()
                  << " beats, " << pattern.num_lanes() << " lane, "
                  << seq.size() << " notes -> " << compiled_events.size() << " TimelineEvents)\n"
                  << "Compiled Events (Control-side log):\n";

        for (const auto& ev : compiled_events) {
            std::cout << "  Sample " << std::setw(7) << ev.sample_position << ": ";
            if (std::holds_alternative<saudade::events::NoteOn>(ev.payload)) {
                const auto& on = std::get<saudade::events::NoteOn>(ev.payload);
                std::cout << "NoteOn  [ID " << on.note_id << "] Pitch: " << on.pitch << " Vel: " << on.velocity << "\n";
            } else {
                const auto& off = std::get<saudade::events::NoteOff>(ev.payload);
                std::cout << "NoteOff [ID " << off.note_id << "] RelVel: " << off.release_velocity << "\n";
            }
        }

        std::cout << "==================================================\n"
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
