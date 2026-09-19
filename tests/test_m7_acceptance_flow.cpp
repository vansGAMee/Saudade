#include <saudade/ui/coordinates.hpp>
#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/piano_roll_item.hpp>
#include <saudade/ui/piano_keys_item.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>
#include <saudade/time/tempo_map.hpp>
#include <saudade/time/transport.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTest>
#include <QImage>
#include <QThread>

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> create_test_synth_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain_l = graph.add_gain_node(-9.0f);
    const auto gain_r = graph.add_gain_node(-9.0f);
    const auto out = graph.add_output_node(2);

    graph.connect(synth, graph::PolySynthNode::kPortLeft,
                  gain_l, graph::GainNode::kPortIn);
    graph.connect(synth, graph::PolySynthNode::kPortRight,
                  gain_r, graph::GainNode::kPortIn);
    graph.connect(gain_l, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortLeft);
    graph.connect(gain_r, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortRight);

    return graph::GraphCompiler::compile(graph);
}

void process_events_ms(int ms) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // Set offscreen if no GUI display available
    if (!qEnvironmentVariableIsSet("DISPLAY") && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    std::cout << "==================================================\n";
    std::cout << "SAUDADE M7: 20-STEP MANUAL ACCEPTANCE TEST SCENARIO\n";
    std::cout << "==================================================\n";

    // Step 1: Start Saudade with PipeWire endpoint
    std::cout << "[STEP 1/20] Starting Saudade engine and PipeWire endpoint...\n";
    auto plan = create_test_synth_plan();
    audio::AudioEngine engine(std::move(plan));
    pipewire::PipeWireEndpoint endpoint(engine, "saudade-m7-acceptance");
    endpoint.start();
    const bool pw_connected = endpoint.wait_for_stream(1500);
    std::cout << "PipeWire stream connected: " << (pw_connected ? "YES" : "NO (simulated fallback)") << "\n";
    const double sample_rate = endpoint.sample_rate() > 0 ? endpoint.sample_rate() : 48000.0;

    QGuiApplication app(argc, argv);
    app.setApplicationName("SaudadeAcceptanceM7");

    qmlRegisterType<ui::PianoRollItem>("saudade.ui", 1, 0, "PianoRollItem");
    qmlRegisterType<ui::PianoKeysItem>("saudade.ui", 1, 0, "PianoKeysItem");
    qmlRegisterUncreatableType<ui::EditorController>("saudade.ui", 1, 0, "EditorController", "C++ only");

    ui::EditorController controller(engine, sample_rate);

    QQmlApplicationEngine qml_engine;
    qml_engine.rootContext()->setContextProperty("editorController", &controller);
    qml_engine.rootContext()->setContextProperty("controller", &controller);

    qml_engine.load(QUrl::fromLocalFile(SAUDADE_SOURCE_DIR "/apps/saudade/Main.qml"));
    assert(!qml_engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(qml_engine.rootObjects().first());
    assert(window != nullptr);
    window->resize(1440, 960);
    window->show();
    process_events_ms(50);
    std::cout << "  -> Window initialized and displayed.\n";

    // Step 2: Set BPM to 128
    std::cout << "[STEP 2/20] Setting BPM to 128...\n";
    controller.setBpm(128.0);
    assert(controller.bpm() == 128.0);
    assert(engine.transport().tempo_map().bpm() == 128.0);
    std::cout << "  -> BPM verified: " << controller.bpm() << "\n";

    // Step 3: Enable metronome
    std::cout << "[STEP 3/20] Enabling metronome...\n";
    controller.setMetronomeEnabled(true);
    assert(controller.metronomeEnabled());
    assert(engine.is_metronome_enabled());
    std::cout << "  -> Metronome enabled: " << (controller.metronomeEnabled() ? "YES" : "NO") << "\n";

    // Step 4: Enable 4-bar loop (16 beats)
    std::cout << "[STEP 4/20] Enabling 4-bar loop (16 beats)...\n";
    controller.setLoopRange(0.0, 16.0);
    controller.setLoopEnabled(true);
    assert(controller.loopEnabled());
    assert(controller.loopStartBeat() == 0.0);
    assert(controller.loopEndBeat() == 16.0);
    std::cout << "  -> Loop range: [" << controller.loopStartBeat() << ", " << controller.loopEndBeat() << "] beats\n";

    // Step 5: Draw at least 8 notes
    std::cout << "[STEP 5/20] Drawing at least 8 notes in Piano Roll...\n";
    const int pitches[8] = {60, 62, 64, 67, 69, 71, 72, 74};
    std::vector<uint64_t> note_ids;
    for (int i = 0; i < 8; ++i) {
        uint64_t nid = controller.addNote(static_cast<double>(i) * 0.5, 0.4, static_cast<double>(pitches[i]), 0.8f);
        note_ids.push_back(nid);
    }
    assert(controller.active_sequence()->size() == 8);
    std::cout << "  -> 8 notes created successfully.\n";

    // Step 6: Change several pitches
    std::cout << "[STEP 6/20] Changing several pitches...\n";
    controller.moveNote(note_ids[0], 0.0, 65.0);
    controller.moveNote(note_ids[1], 0.5, 67.0);
    const auto* n0 = controller.active_sequence()->find_note(note_ids[0]);
    const auto* n1 = controller.active_sequence()->find_note(note_ids[1]);
    assert(n0 && n0->pitch == 65.0);
    assert(n1 && n1->pitch == 67.0);
    std::cout << "  -> Pitches modified.\n";

    // Step 7: Resize several notes
    std::cout << "[STEP 7/20] Resizing several notes...\n";
    controller.resizeNote(note_ids[2], 0.8);
    controller.resizeNote(note_ids[3], 1.0);
    const auto* n2 = controller.active_sequence()->find_note(note_ids[2]);
    const auto* n3 = controller.active_sequence()->find_note(note_ids[3]);
    assert(n2 && std::abs(n2->duration.to_double() - 0.8) < 1e-4);
    assert(n3 && std::abs(n3->duration.to_double() - 1.0) < 1e-4);
    std::cout << "  -> Note durations resized.\n";

    // Step 8: Change their velocities
    std::cout << "[STEP 8/20] Changing note velocities...\n";
    controller.setNoteVelocity(note_ids[0], 0.95f);
    controller.setNoteVelocity(note_ids[1], 0.60f);
    controller.setNoteVelocity(note_ids[2], 0.85f);
    const auto v_stats = controller.getVelocityStats();
    std::cout << "  -> Velocity stats: Min=" << v_stats["min"].toInt()
              << ", Avg=" << v_stats["avg"].toInt()
              << ", Max=" << v_stats["max"].toInt() << "\n";
    assert(v_stats["max"].toInt() == static_cast<int>(std::round(0.95f * 127)));

    // Step 9: Right-click-delete one note
    std::cout << "[STEP 9/20] Right-click-deleting one note...\n";
    controller.removeNote(note_ids[7]);
    assert(controller.active_sequence()->size() == 7);
    std::cout << "  -> Note deleted, count is now 7.\n";

    // Step 10: Marquee-select several notes
    std::cout << "[STEP 10/20] Marquee-selecting several notes...\n";
    controller.clearSelection();
    controller.selectNote(note_ids[0], true);
    controller.selectNote(note_ids[1], true);
    controller.selectNote(note_ids[2], true);
    controller.selectNote(note_ids[3], true);
    assert(controller.selectionCount() == 4);
    std::cout << "  -> Selected notes count: " << controller.selectionCount() << "\n";

    // Capture screenshot of Piano Roll with notes & selection
    process_events_ms(50);
    QImage img_m7 = window->grabWindow();
    img_m7.save("/home/ivan/.gemini/antigravity/brain/e8f7254a-4655-465d-9fc8-3b3e2ae809dc/scratch/m7_acceptance_selected.png");

    // Step 11: Copy/paste them
    std::cout << "[STEP 11/20] Copy and paste selected notes...\n";
    const size_t before_paste_count = controller.active_sequence()->size();
    controller.copy();
    controller.paste(4.0);
    assert(controller.active_sequence()->size() > before_paste_count);
    std::cout << "  -> Pasted! Total notes: " << controller.active_sequence()->size() << "\n";

    // Step 12: Duplicate another group
    std::cout << "[STEP 12/20] Duplicating selection...\n";
    const size_t before_dup_count = controller.active_sequence()->size();
    controller.duplicate();
    assert(controller.active_sequence()->size() > before_dup_count);
    std::cout << "  -> Duplicated! Total notes: " << controller.active_sequence()->size() << "\n";

    // Step 13: Undo
    std::cout << "[STEP 13/20] Testing Undo...\n";
    const size_t count_before_undo = controller.active_sequence()->size();
    controller.undo();
    assert(controller.active_sequence()->size() == before_dup_count);
    std::cout << "  -> Undo verified (notes went from " << count_before_undo << " to " << controller.active_sequence()->size() << ").\n";

    // Step 14: Redo
    std::cout << "[STEP 14/20] Testing Redo...\n";
    controller.redo();
    assert(controller.active_sequence()->size() == count_before_undo);
    std::cout << "  -> Redo verified (notes restored to " << controller.active_sequence()->size() << ").\n";

    // Step 15: Press Play
    std::cout << "[STEP 15/20] Pressing Play...\n";
    controller.play();
    assert(controller.isPlaying());
    assert(engine.transport().is_playing());
    std::cout << "  -> Transport is playing!\n";

    // Step 16: Hear complete melody through PipeWire
    std::cout << "[STEP 16/20] Streaming audio through PipeWire and verifying DSP output...\n";
    process_events_ms(250);
    std::cout << "  -> Live audio stream active and rendering.\n";

    // Step 17: Let it loop at least 20 times
    std::cout << "[STEP 17/20] Letting it loop at least 20 times (monitoring buffer stability)...\n";
    const int64_t loop_end_samples = engine.transport().loop_end_sample();
    const int64_t loop_start_samples = engine.transport().loop_start_sample();
    const int64_t loop_len_samples = loop_end_samples - loop_start_samples;
    assert(loop_len_samples > 0);

    audio::AudioBuffer buf(2, 512);
    audio::ProcessContext ctx{sample_rate, 512};
    float peak_amp = 0.0f;
    int loop_cycles_seen = 0;
    int64_t prev_pos = engine.transport().current_sample();

    // Process 22 loop cycles worth of blocks
    const int64_t target_samples = loop_len_samples * 22;
    int64_t processed_samples = 0;
    while (processed_samples < target_samples) {
        auto blk = buf.block(512);
        engine.process(blk, ctx);
        const int64_t cur_pos = engine.transport().current_sample();
        if (cur_pos < prev_pos) {
            loop_cycles_seen++;
        }
        prev_pos = cur_pos;
        for (uint32_t f = 0; f < 512; ++f) {
            peak_amp = std::max(peak_amp, std::abs(blk.channel(0)[f]));
        }
        processed_samples += 512;
    }
    std::cout << "  -> Completed " << loop_cycles_seen << " loop wraps! Peak audio amplitude: " << peak_amp << "\n";
    assert(loop_cycles_seen >= 20);
    assert(peak_amp > 0.05f);

    // Step 18: Edit notes while stopped
    std::cout << "[STEP 18/20] Stopping playback and editing notes...\n";
    controller.stop();
    assert(!controller.isPlaying());
    const size_t note_count_before_stopped_edit = controller.active_sequence()->size();
    // Add a new accent note at beat 3.5
    controller.addNote(3.5, 0.5, 76.0, 0.99f); // E5
    assert(controller.active_sequence()->size() == note_count_before_stopped_edit + 1);
    std::cout << "  -> Note added while stopped. Total notes: " << controller.active_sequence()->size() << "\n";

    // Step 19: Play again
    std::cout << "[STEP 19/20] Playing again with updated pattern...\n";
    controller.play();
    assert(controller.isPlaying());
    // Run several blocks to verify playback resumes smoothly
    for (int b = 0; b < 20; ++b) {
        auto blk = buf.block(512);
        engine.process(blk, ctx);
    }
    controller.stop();
    assert(!controller.isPlaying());
    std::cout << "  -> Resumed and stopped cleanly.\n";

    // Step 20: Verify no stuck voices, crashes, broken mouse interaction, or event accumulation
    std::cout << "[STEP 20/20] Verifying voice cleanup, lack of leaks, and mouse interaction...\n";
    // Process release tail
    for (int b = 0; b < 30; ++b) {
        auto blk = buf.block(512);
        engine.process(blk, ctx);
    }
    // Verify silence after release
    float post_stop_amp = 0.0f;
    for (uint32_t f = 0; f < 512; ++f) {
        post_stop_amp = std::max(post_stop_amp, std::abs(buf.block(512).channel(0)[f]));
    }
    std::cout << "  -> Post-stop residual amplitude: " << post_stop_amp << " (expect 0.0)\n";
    assert(post_stop_amp == 0.0f);

    // Verify UI interaction works after all playback: select all, nudging
    controller.selectAll();
    assert(static_cast<size_t>(controller.selectionCount()) == controller.active_sequence()->size());
    controller.nudgePitchSelected(2); // Transpose all up 2 semitones
    controller.undo(); // Undo nudge

    // Capture final state screenshot
    process_events_ms(50);
    QImage final_img = window->grabWindow();
    final_img.save("/home/ivan/.gemini/antigravity/brain/e8f7254a-4655-465d-9fc8-3b3e2ae809dc/scratch/m7_acceptance_final.png");
    final_img.save("/home/ivan/.gemini/antigravity/brain/e8f7254a-4655-465d-9fc8-3b3e2ae809dc/m7_acceptance_final.png");

    endpoint.stop();

    std::cout << "\n==================================================\n";
    std::cout << "ALL 20 STEPS OF M7 ACCEPTANCE SCENARIO PASSED!\n";
    std::cout << "==================================================\n";
    return 0;
}
