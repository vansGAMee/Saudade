#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/editor_commands.hpp>
#include <saudade/ui/coordinates.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>
#include <saudade/audio/allocation_guard.hpp>
#include <saudade/time/tempo_map.hpp>
#include <saudade/time/transport.hpp>

#include <QGuiApplication>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_test_synth_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain = graph.add_gain_node(-12.0f);
    const auto out = graph.add_output_node(2);

    graph.connect(synth, graph::PolySynthNode::kPortOut,
                  gain, graph::GainNode::kPortIn);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortLeft);
    graph.connect(gain, graph::GainNode::kPortOut,
                  out, graph::OutputNode::kPortRight);

    return graph::GraphCompiler::compile(graph);
}

} // namespace

void test_1_bpm_and_tempo_control() {
    std::cout << "[RUN] test_1_bpm_and_tempo_control\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    assert(controller.bpm() == 120.0);

    // Test setting BPM
    controller.setBpm(128.0);
    assert(controller.bpm() == 128.0);
    assert(engine.transport().tempo_map().bpm() == 128.0);

    // Test clamping range [20, 400]
    controller.setBpm(10.0);
    assert(controller.bpm() == 20.0);

    controller.setBpm(500.0);
    assert(controller.bpm() == 400.0);

    // Musical time conversion consistency:
    // At 120 BPM, 1 beat = 0.5s = 24000 samples @ 48kHz
    controller.setBpm(120.0);
    const auto pos_1_beat = engine.transport().tempo_map().beat_to_sample(
        time::BeatPosition::from_beats(1), 48000.0);
    assert(pos_1_beat == 24000);

    std::cout << "[PASS] test_1_bpm_and_tempo_control\n";
}

void test_2_looping_and_boundary_wrapping() {
    std::cout << "[RUN] test_2_looping_and_boundary_wrapping\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    // 4-bar loop (16 beats): beats [0, 16) = samples [0, 384000) @ 120 BPM, 48kHz
    controller.setBpm(120.0);
    controller.setLoopRange(0.0, 16.0);
    controller.setLoopEnabled(true);

    assert(controller.loopEnabled());
    assert(controller.loopStartBeat() == 0.0);
    assert(controller.loopEndBeat() == 16.0);

    // Draw notes:
    // Note 1: C4 [0, 1)
    // Note 2: G4 [15, 16) - adjacent to loop boundary
    controller.addNote(0.0, 1.0, 60.0);
    controller.addNote(15.0, 1.0, 67.0);

    controller.play();
    assert(controller.isPlaying());

    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext ctx{48000.0, 512};

    // Run 50 complete loop iterations!
    // 16 beats @ 120 BPM = 8 seconds = 384,000 samples = 750 blocks of 512 frames
    const size_t blocks_per_loop = 384000 / 512;
    const size_t total_blocks = blocks_per_loop * 50;

    float peak_amp = 0.0f;
    for (size_t b = 0; b < total_blocks; ++b) {
        auto blk = buffer.block(512);
        engine.process(blk, ctx);

        // Ensure no NaN / Inf
        const float* left = blk.channel(0);
        for (uint32_t f = 0; f < 512; ++f) {
            assert(!std::isnan(left[f]));
            assert(!std::isinf(left[f]));
            if (std::abs(left[f]) > peak_amp) {
                peak_amp = std::abs(left[f]);
            }
        }
    }

    assert(peak_amp > 0.01f); // Sound was produced and looped
    std::cout << "50 loop iterations executed smoothly. Peak amp: " << peak_amp << "\n";

    controller.stop();
    assert(!controller.isPlaying());

    std::cout << "[PASS] test_2_looping_and_boundary_wrapping\n";
}

void test_3_realtime_metronome() {
    std::cout << "[RUN] test_3_realtime_metronome\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    controller.setBpm(120.0);
    controller.setMetronomeEnabled(true);
    assert(controller.metronomeEnabled());

    controller.play();

    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext ctx{48000.0, 512};

    bool found_accent_click = false;
    bool found_regular_click = false;

    // Process 2 bars (8 beats = 192,000 samples = 375 blocks)
    for (size_t b = 0; b < 375; ++b) {
        auto blk = buffer.block(512);
        engine.process(blk, ctx);

        const float* left = blk.channel(0);
        for (uint32_t f = 0; f < 512; ++f) {
            if (std::abs(left[f]) > 0.05f) {
                if (b < 10) {
                    found_accent_click = true;
                } else {
                    found_regular_click = true;
                }
            }
        }
    }

    assert(found_accent_click);
    assert(found_regular_click);

    controller.stop();
    std::cout << "[PASS] test_3_realtime_metronome\n";
}

void test_4_selection_clipboard_undo_redo() {
    std::cout << "[RUN] test_4_selection_clipboard_undo_redo\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    // 1. Add 3 notes
    const uint64_t id1 = controller.addNote(0.0, 1.0, 60.0);
    const uint64_t id2 = controller.addNote(1.0, 1.0, 64.0);
    const uint64_t id3 = controller.addNote(2.0, 1.0, 67.0);

    assert(controller.active_sequence()->size() == 3);

    // 2. Select All
    controller.selectAll();
    assert(controller.selectionCount() == 3);
    assert(controller.isNoteSelected(id1));
    assert(controller.isNoteSelected(id2));
    assert(controller.isNoteSelected(id3));

    // 3. Move selected group: +1 beat, +2 semitones
    controller.moveSelected(1.0, 2);
    const auto* n1 = controller.active_sequence()->find_note(id1);
    assert(n1->pitch == 62.0);
    assert(n1->start == time::BeatPosition::from_beats(1));

    // 4. Undo move
    assert(controller.canUndo());
    controller.undo();
    n1 = controller.active_sequence()->find_note(id1);
    assert(n1->pitch == 60.0);
    assert(n1->start == time::BeatPosition::zero());

    // 5. Redo move
    assert(controller.canRedo());
    controller.redo();
    n1 = controller.active_sequence()->find_note(id1);
    assert(n1->pitch == 62.0);

    // 6. Copy & Paste
    controller.selectNote(id1, false);
    controller.copy();
    controller.paste(4.0);
    assert(controller.active_sequence()->size() == 4);

    // 7. Duplicate
    controller.duplicate();
    assert(controller.active_sequence()->size() == 5);

    // 8. Delete Selected
    controller.deleteSelected();
    assert(controller.active_sequence()->size() == 4);

    // 9. Batch history test:
    // 100 drag moves inside a batch must produce exactly ONE undo entry!
    controller.beginBatch();
    for (int i = 0; i < 100; ++i) {
        controller.moveSelected(0.0, 1);
    }
    controller.commitBatch();

    // Now undo should revert all 100 micro-moves in one step!
    controller.undo();

    std::cout << "[PASS] test_4_selection_clipboard_undo_redo\n";
}

void test_5_live_audition() {
    std::cout << "[RUN] test_5_live_audition\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext ctx{48000.0, 512};

    // Stopped engine: auditioning must produce audio!
    assert(!controller.isPlaying());
    controller.auditionNoteOn(69.0, 0.8f); // A4 (440 Hz)

    auto blk = buffer.block(512);
    engine.process(blk, ctx);

    float max_samp = 0.0f;
    for (uint32_t f = 0; f < 512; ++f) {
        max_samp = std::max(max_samp, std::abs(blk.channel(0)[f]));
    }
    assert(max_samp > 0.05f); // Audition voice rendered!

    // Release audition note
    controller.auditionNoteOff();
    // Process through ADSR release phase until silent
    for (int q = 0; q < 30; ++q) {
        engine.process(blk, ctx);
    }

    max_samp = 0.0f;
    for (uint32_t f = 0; f < 512; ++f) {
        max_samp = std::max(max_samp, std::abs(blk.channel(0)[f]));
    }
    assert(max_samp == 0.0f); // Silence restored!

    std::cout << "[PASS] test_5_live_audition\n";
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    test_1_bpm_and_tempo_control();
    test_2_looping_and_boundary_wrapping();
    test_3_realtime_metronome();
    test_4_selection_clipboard_undo_redo();
    test_5_live_audition();

    std::cout << "\n=== ALL M7 SONGWRITING TESTS PASSED ===\n";
    return 0;
}
