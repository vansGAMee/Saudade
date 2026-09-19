#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/piano_roll_item.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <QGuiApplication>
#include <QQuickWindow>

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain_l = graph.add_gain_node(-12.0f);
    const auto gain_r = graph.add_gain_node(-12.0f);
    const auto output = graph.add_output_node(2);
    graph.connect(synth, graph::PolySynthNode::kPortLeft,
                  gain_l, graph::GainNode::kPortIn);
    graph.connect(synth, graph::PolySynthNode::kPortRight,
                  gain_r, graph::GainNode::kPortIn);
    graph.connect(gain_l, graph::GainNode::kPortOut,
                  output, graph::OutputNode::kPortLeft);
    graph.connect(gain_r, graph::GainNode::kPortOut,
                  output, graph::OutputNode::kPortRight);
    return graph::GraphCompiler::compile(graph);
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);

    audio::AudioEngine engine(make_plan());
    ui::EditorController controller(engine, 48000.0);
    auto* sequence = controller.active_sequence();
    assert(sequence != nullptr);

    const auto build_start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        const auto beat_ticks =
            static_cast<int64_t>(i % 64) *
            (time::BeatPosition::kTicksPerBeat / 4);
        const auto id = sequence->add_note(
            time::BeatPosition::from_ticks(beat_ticks),
            time::BeatDuration::from_fraction(1, 8),
            48.0 + static_cast<double>(i % 36),
            0.35f + static_cast<float>(i % 60) / 100.0f);
        assert(id.has_value());
    }
    controller.syncPatternToEngine();
    const auto initial_clip =
        controller.clipsData().front().toMap().value("id").toULongLong();
    assert(controller.duplicateClip(initial_clip) != 0);
    assert(controller.duplicateClip(initial_clip) != 0);
    assert(controller.duplicateClip(initial_clip) != 0);
    assert(controller.clipsData().size() == 4);
    assert(engine.active_track().count == 8000);
    const auto build_end = std::chrono::steady_clock::now();

    QQuickWindow window;
    window.resize(1280, 720);
    ui::PianoRollItem roll(window.contentItem());
    roll.setController(&controller);
    roll.setWidth(1160);
    roll.setHeight(700);
    roll.setBeatWidth(80.0f);
    roll.setRowHeight(18.0f);
    window.show();
    QCoreApplication::processEvents();
    const QImage frame_a = window.grabWindow();
    assert(!frame_a.isNull());
    roll.setBeatWidth(150.0f);
    roll.setRowHeight(24.0f);
    QCoreApplication::processEvents();
    const QImage frame_b = window.grabWindow();
    assert(!frame_b.isNull());

    controller.selectAll();
    controller.moveSelected(0.25, 1);
    assert(controller.selectionCount() == 1000);
    controller.undo();
    controller.redo();

    controller.setLoopRange(0.0, 16.0);
    controller.setLoopEnabled(true);
    controller.play();
    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext context{48000.0, 512};
    float max_peak = 0.0f;
    for (int block_index = 0; block_index < 1000; ++block_index) {
        auto block = buffer.block(512);
        engine.process(block, context);
        for (uint32_t frame = 0; frame < 512; ++frame) {
            max_peak = std::max(
                max_peak, std::abs(buffer.channel(0)[frame]));
        }
    }
    controller.stop();
    assert(max_peak > 0.001f);
    assert(std::isfinite(max_peak));
    assert(engine.telemetry().sequence >= 1000);

    const auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        build_end - build_start).count();
    std::cout << "BETA STRESS PASSED: 1000 notes, 4 clips, 8000 events; compile "
              << build_ms << " ms\n";
    return 0;
}
