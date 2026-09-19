#include <saudade/ui/editor_controller.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace saudade;

namespace {

std::unique_ptr<renderplan::RenderPlan> make_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain_l = graph.add_gain_node(-9.0f);
    const auto gain_r = graph.add_gain_node(-9.0f);
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

void be16(std::vector<char>& out, uint16_t value) {
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<char>(value & 0xFFU));
}

void be32(std::vector<char>& out, uint32_t value) {
    out.push_back(static_cast<char>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<char>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<char>(value & 0xFFU));
}

QByteArray midi_fixture() {
    const std::vector<char> track{
        0x00, static_cast<char>(0xFF), 0x03, 0x06,
        'I', 'm', 'p', 'o', 'r', 't',
        0x00, static_cast<char>(0x90), 72, 100,
        static_cast<char>(0x83), 0x60,
        static_cast<char>(0x80), 72, 64,
        0x00, static_cast<char>(0xFF), 0x2F, 0x00,
    };
    std::vector<char> file{'M', 'T', 'h', 'd'};
    be32(file, 6);
    be16(file, 0);
    be16(file, 1);
    be16(file, 480);
    file.insert(file.end(), {'M', 'T', 'r', 'k'});
    be32(file, static_cast<uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());
    return QByteArray(file.data(), static_cast<qsizetype>(file.size()));
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    assert(directory.isValid());

    audio::AudioEngine engine(make_plan());
    ui::EditorController controller(engine, 48000.0);
    controller.setBpm(128.0);
    controller.setMetronomeEnabled(true);
    controller.setLoopRange(0.0, 16.0);
    controller.setLoopEnabled(true);

    for (int i = 0; i < 16; ++i) {
        const double start = static_cast<double>(i) * 0.5;
        const double pitch = 60.0 + static_cast<double>((i * 5) % 12);
        assert(controller.addNote(start, 0.375, pitch,
                                  0.55f + static_cast<float>(i % 4) * 0.1f) != 0);
    }
    assert(controller.active_sequence()->size() == 16);
    controller.selectAll();
    controller.moveSelected(0.25, 1);
    controller.resizeSelected(0.125);
    controller.setSelectedVelocity(0.76f);
    controller.copy();
    controller.paste(8.0);
    assert(controller.active_sequence()->size() == 32);
    controller.duplicate();
    assert(controller.active_sequence()->size() == 48);
    controller.deleteSelected();
    assert(controller.active_sequence()->size() == 32);
    controller.undo();
    assert(controller.active_sequence()->size() == 48);
    controller.redo();
    assert(controller.active_sequence()->size() == 32);

    const auto first_track =
        controller.tracksData().front().toMap().value("id").toULongLong();
    const auto first_clip =
        controller.clipsData().front().toMap().value("id").toULongLong();
    assert(controller.duplicateClip(first_clip) != 0);
    const auto second_track = controller.addTrack("Countermelody");
    const auto second_clip = controller.createPattern(second_track, 16.0, 8.0);
    assert(second_clip != 0);
    for (int i = 0; i < 8; ++i) {
        assert(controller.addNote(i * 0.75, 0.5, 48.0 + (i % 5) * 3, 0.65f) != 0);
    }
    assert(controller.setTrackGain(first_track, -4.0f));
    assert(controller.setTrackPan(first_track, -0.35f));
    assert(controller.setTrackGain(second_track, -7.0f));
    assert(controller.setTrackPan(second_track, 0.45f));
    controller.setMasterGainDb(-1.5f);
    controller.setSynthAttack(0.015f);
    controller.setSynthCutoff(4800.0f);
    controller.setSynthRelease(0.42f);

    const QString midi_path = directory.filePath("material.mid");
    QFile midi_file(midi_path);
    assert(midi_file.open(QIODevice::WriteOnly));
    assert(midi_file.write(midi_fixture()) > 0);
    midi_file.close();
    const auto tracks_before_import = controller.project().tracks().size();
    assert(controller.importMidi(midi_path, 24.0));
    assert(controller.project().tracks().size() == tracks_before_import + 1);
    assert(controller.active_sequence()->size() == 1);

    controller.seekBeats(0.0);
    controller.play();
    audio::AudioBuffer buffer(2, 512);
    audio::ProcessContext context{48000.0, 512};
    const int frames_per_loop =
        static_cast<int>(std::llround(16.0 * 60.0 / 128.0 * 48000.0));
    const int blocks = (frames_per_loop * 21) / 512 + 2;
    float peak = 0.0f;
    for (int i = 0; i < blocks; ++i) {
        auto block = buffer.block(512);
        engine.process(block, context);
        for (uint32_t frame = 0; frame < 512; ++frame) {
            peak = std::max(peak, std::abs(buffer.channel(0)[frame]));
            assert(std::isfinite(buffer.channel(0)[frame]));
            assert(std::isfinite(buffer.channel(1)[frame]));
        }
    }
    assert(peak > 0.001f);
    assert(engine.telemetry().peak_left >= 0.0f);
    controller.stop();
    for (int i = 0; i < 100; ++i) {
        auto block = buffer.block(512);
        engine.process(block, context);
    }
    assert(engine.telemetry().peak_left == 0.0f);
    assert(engine.telemetry().peak_right == 0.0f);

    const QString project_path = directory.filePath("beta.dawproj");
    assert(controller.saveProjectAs(project_path));
    assert(!controller.projectDirty());

    audio::AudioEngine reopened_engine(make_plan());
    ui::EditorController reopened(reopened_engine, 48000.0);
    assert(reopened.openProject(project_path));
    assert(reopened.bpm() == 128.0);
    assert(reopened.project().tracks().size() == controller.project().tracks().size());
    assert(reopened.project().clips().size() == controller.project().clips().size());
    assert(std::abs(reopened.masterGainDb() + 1.5f) < 0.01f);
    assert(std::abs(reopened.synthCutoff() - 4800.0f) < 0.01f);
    assert(std::abs(reopened.synthRelease() - 0.42f) < 0.001f);

    const QString wav_path = directory.filePath("beta.wav");
    assert(reopened.exportWav(wav_path));
    QFile wav(wav_path);
    assert(wav.open(QIODevice::ReadOnly));
    const QByteArray wav_bytes = wav.readAll();
    assert(wav_bytes.size() > 44);
    assert(std::memcmp(wav_bytes.constData(), "RIFF", 4) == 0);
    bool non_zero = false;
    for (qsizetype i = 44; i < wav_bytes.size(); ++i) {
        if (wav_bytes[i] != 0) {
            non_zero = true;
            break;
        }
    }
    assert(non_zero);

    std::cout << "BETA END-TO-END WORKFLOW PASSED\n";
    return 0;
}
