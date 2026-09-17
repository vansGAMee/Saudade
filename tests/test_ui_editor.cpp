#include <saudade/ui/coordinates.hpp>
#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/piano_roll_item.hpp>
#include <saudade/ui/piano_keys_item.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cassert>
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

void test_a_coordinate_mapping() {
    std::cout << "[RUN] test_a_coordinate_mapping\n";

    using ui::Coordinates;

    // 1. Beat to X and X to Beat
    const float beat_w = 160.0f;
    assert(Coordinates::beat_to_x(time::BeatPosition::zero(), beat_w) == 0.0f);
    assert(Coordinates::beat_to_x(time::BeatPosition::from_beats(1), beat_w) == 160.0f);
    assert(Coordinates::beat_to_x(time::BeatPosition::from_fraction(1, 2), beat_w) == 80.0f);

    assert(Coordinates::x_to_beat(0.0f, beat_w) == time::BeatPosition::zero());
    assert(Coordinates::x_to_beat(160.0f, beat_w) == time::BeatPosition::from_beats(1));
    assert(Coordinates::x_to_beat(80.0f, beat_w) == time::BeatPosition::from_fraction(1, 2));
    assert(Coordinates::x_to_beat(-10.0f, beat_w) == time::BeatPosition::zero()); // clamped >= 0

    // 2. Pitch to Y and Y to Pitch
    const float row_h = 20.0f;
    const int max_p = 84; // C6
    const int min_p = 48; // C3
    assert(Coordinates::pitch_to_y(84.0, row_h, max_p) == 0.0f);
    assert(Coordinates::pitch_to_y(60.0, row_h, max_p) == (84 - 60) * 20.0f); // 480.0f
    assert(Coordinates::pitch_to_y(48.0, row_h, max_p) == (84 - 48) * 20.0f); // 720.0f

    assert(Coordinates::y_to_pitch(0.0f, row_h, max_p) == 84.0);
    assert(Coordinates::y_to_pitch(480.0f, row_h, max_p) == 60.0);
    assert(Coordinates::y_to_pitch(720.0f, row_h, max_p) == 48.0);

    // 3. Quantize Pitch
    assert(Coordinates::quantize_pitch(5.0f, row_h, min_p, max_p) == 84);
    assert(Coordinates::quantize_pitch(19.9f, row_h, min_p, max_p) == 84);
    assert(Coordinates::quantize_pitch(20.1f, row_h, min_p, max_p) == 83);
    assert(Coordinates::quantize_pitch(485.0f, row_h, min_p, max_p) == 60);

    // 4. Quantize Beat & Floor Quantize Beat (1/4 beat = 240,000 ticks)
    const auto q_step = time::BeatDuration::from_fraction(1, 4);
    assert(Coordinates::quantize_beat(time::BeatPosition::zero(), q_step) == time::BeatPosition::zero());
    assert(Coordinates::quantize_beat(time::BeatPosition::from_ticks(100000), q_step) == time::BeatPosition::zero());
    assert(Coordinates::quantize_beat(time::BeatPosition::from_ticks(150000), q_step) == time::BeatPosition::from_ticks(240000));
    assert(Coordinates::floor_quantize_beat(time::BeatPosition::from_ticks(350000), q_step) == time::BeatPosition::from_ticks(240000));

    // 5. Quantize Duration
    assert(Coordinates::quantize_duration(time::BeatDuration::from_ticks(0), q_step) == q_step);
    assert(Coordinates::quantize_duration(time::BeatDuration::from_ticks(200000), q_step) == q_step);
    assert(Coordinates::quantize_duration(time::BeatDuration::from_ticks(380000), q_step) == time::BeatDuration::from_fraction(2, 4));

    // 6. Black key detection & pitch names
    assert(Coordinates::is_black_key(60) == false); // C4
    assert(Coordinates::is_black_key(61) == true);  // C#4
    assert(Coordinates::pitch_name(60) == "C4");
    assert(Coordinates::pitch_name(48) == "C3");
    assert(Coordinates::pitch_name(84) == "C6");

    std::cout << "[PASS] test_a_coordinate_mapping\n";
}

void test_b_c_d_e_note_crud_operations() {
    std::cout << "[RUN] test_b_c_d_e_note_crud_operations\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    assert(controller.active_sequence()->empty());

    // Test B: Create Note
    const uint64_t id1 = controller.addNote(0.0, 1.0, 60.0, 0.8f);
    assert(id1 == 1);
    assert(controller.active_sequence()->size() == 1);

    const auto* n1 = controller.active_sequence()->find_note(id1);
    assert(n1 != nullptr);
    assert(n1->start == time::BeatPosition::zero());
    assert(n1->duration == time::BeatDuration::from_beats(1));
    assert(n1->pitch == 60.0);
    assert(n1->velocity == 0.8f);

    // Test C: Move Note (preserving NoteId)
    assert(controller.moveNote(id1, 2.0, 64.0));
    const auto* n1_moved = controller.active_sequence()->find_note(id1);
    assert(n1_moved != nullptr);
    assert(n1_moved->note_id == id1);
    assert(n1_moved->start == time::BeatPosition::from_beats(2));
    assert(n1_moved->pitch == 64.0);

    // Test D: Resize Note (preserving NoteId, duration > 0)
    assert(controller.resizeNote(id1, 0.5));
    const auto* n1_resized = controller.active_sequence()->find_note(id1);
    assert(n1_resized != nullptr);
    assert(n1_resized->note_id == id1);
    assert(n1_resized->duration == time::BeatDuration::from_fraction(1, 2));

    // Reject non-positive resize
    assert(!controller.resizeNote(id1, 0.0));
    assert(!controller.resizeNote(id1, -0.5));
    assert(controller.active_sequence()->find_note(id1)->duration == time::BeatDuration::from_fraction(1, 2));

    // Test E: Delete Note
    const uint64_t id2 = controller.addNote(1.0, 0.5, 67.0);
    assert(id2 == 2);
    assert(controller.active_sequence()->size() == 2);

    // Delete id1 only
    assert(controller.removeNote(id1));
    assert(controller.active_sequence()->size() == 1);
    assert(controller.active_sequence()->find_note(id1) == nullptr);
    assert(controller.active_sequence()->find_note(id2) != nullptr);

    std::cout << "[PASS] test_b_c_d_e_note_crud_operations\n";
}

void test_f_playback_preparation() {
    std::cout << "[RUN] test_f_playback_preparation\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    // User draws C4 (beat 0 -> 1) and E4 (beat 1 -> 2)
    controller.addNote(0.0, 1.0, 60.0);
    controller.addNote(1.0, 1.0, 64.0);

    const auto compiled = model::PatternCompiler::compile(
        controller.pattern(),
        engine.transport().tempo_map(),
        48000.0
    );

    assert(compiled.size() == 4);
    // Note 1 On at sample 0, Off at sample 24000
    assert(compiled[0].sample_position == 0);
    assert(std::holds_alternative<events::NoteOn>(compiled[0].payload));
    assert(compiled[1].sample_position == 24000);
    assert(std::holds_alternative<events::NoteOff>(compiled[1].payload));

    // Note 2 On at sample 24000, Off at sample 48000
    assert(compiled[2].sample_position == 24000);
    assert(std::holds_alternative<events::NoteOn>(compiled[2].payload));
    assert(compiled[3].sample_position == 48000);
    assert(std::holds_alternative<events::NoteOff>(compiled[3].payload));

    std::cout << "[PASS] test_f_playback_preparation\n";
}

void test_g_h_repeated_playback_and_rt_safe_flush() {
    std::cout << "[RUN] test_g_h_repeated_playback_and_rt_safe_flush\n";

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext ctx{48000.0, 512};

    // Cycle 1: Draw note C4 [0, 1) -> Play
    controller.addNote(0.0, 1.0, 60.0);
    controller.play();
    assert(controller.isPlaying());

    // Process 2 quanta on simulated RT thread
    auto block = buffer.block(512);
    engine.process(block, ctx);
    engine.process(block, ctx);

    // Stop
    controller.stop();
    assert(!controller.isPlaying());

    // Edit: Remove C4, Add G4 [0, 1)
    controller.removeNote(1);
    const uint64_t new_id = controller.addNote(0.0, 1.0, 67.0);
    assert(new_id == 2);

    // Cycle 2: Play again (must flush previous events and schedule only G4)
    // Request flush and simulate RT quantum processing flush
    const uint64_t flush_gen = engine.request_event_flush();
    assert(!engine.is_flush_acknowledged(flush_gen));

    // RT quantum processes the flush
    engine.process(block, ctx);
    assert(engine.is_flush_acknowledged(flush_gen));
    assert(engine.event_queue().empty());

    // Now control calls play()
    controller.play();
    assert(controller.isPlaying());

    // Verify scheduled events in engine queue correspond to new G4 note only
    events::TimelineEvent ev{};
    assert(engine.event_queue().peek(ev));
    assert(std::holds_alternative<events::NoteOn>(ev.payload));
    assert(std::get<events::NoteOn>(ev.payload).note_id == 2);
    assert(std::get<events::NoteOn>(ev.payload).pitch == 67.0);

    std::cout << "[PASS] test_g_h_repeated_playback_and_rt_safe_flush\n";
}

void test_i_qt_gui_smoke_test(int argc, char* argv[]) {
    std::cout << "[RUN] test_i_qt_gui_smoke_test\n";

    // Set offscreen platform for headless smoke testing
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("SaudadeTest");

    qmlRegisterType<ui::PianoRollItem>("saudade.ui", 1, 0, "PianoRollItem");
    qmlRegisterType<ui::PianoKeysItem>("saudade.ui", 1, 0, "PianoKeysItem");

    audio::AudioEngine engine(make_test_synth_plan());
    ui::EditorController controller(engine, 48000.0);

    assert(controller.patternLength() == 4.0);
    assert(controller.bpm() == 120.0);
    assert(!controller.isPlaying());

    // Add a note and verify signal emission
    bool signal_emitted = false;
    QObject::connect(&controller, &ui::EditorController::notesChanged, [&signal_emitted]() {
        signal_emitted = true;
    });

    controller.addNote(0.0, 1.0, 60.0);
    assert(signal_emitted);
    assert(controller.active_sequence()->size() == 1);

    std::cout << "[PASS] test_i_qt_gui_smoke_test\n";
}

int main(int argc, char* argv[]) {
    test_a_coordinate_mapping();
    test_b_c_d_e_note_crud_operations();
    test_f_playback_preparation();
    test_g_h_repeated_playback_and_rt_safe_flush();
    test_i_qt_gui_smoke_test(argc, argv);

    std::cout << "ALL UI & EDITOR TESTS PASSED!\n";
    return 0;
}
