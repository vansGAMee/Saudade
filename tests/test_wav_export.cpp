#include <saudade/ui/project_exporter.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/graph/graph_model.hpp>
#include <saudade/time/tempo_map.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace saudade;

namespace {

uint32_t le32(const char* bytes) {
    return static_cast<uint32_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[1])) << 8U) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[2])) << 16U) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[3])) << 24U);
}

std::unique_ptr<renderplan::RenderPlan> make_plan() {
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node();
    const auto gain_l = graph.add_gain_node(-9.0f);
    const auto gain_r = graph.add_gain_node(-9.0f);
    const auto output = graph.add_output_node(2);
    assert(graph.connect(synth, graph::PolySynthNode::kPortLeft,
                         gain_l, graph::GainNode::kPortIn));
    assert(graph.connect(synth, graph::PolySynthNode::kPortRight,
                         gain_r, graph::GainNode::kPortIn));
    assert(graph.connect(gain_l, graph::GainNode::kPortOut,
                         output, graph::OutputNode::kPortLeft));
    assert(graph.connect(gain_r, graph::GainNode::kPortOut,
                         output, graph::OutputNode::kPortRight));
    return graph::GraphCompiler::compile(graph);
}

} // namespace

int main() {
    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.filePath("render.wav");

    time::TempoMap tempo(120.0);
    std::vector<events::TimelineEvent> events{
        {tempo.beat_to_sample(time::BeatPosition::zero(), 48000.0),
         events::NoteOn{1, 60.0, 0.8f}},
        {tempo.beat_to_sample(time::BeatPosition::from_beats(1), 48000.0),
         events::NoteOff{1, 0.0f}},
    };
    auto plan = make_plan();
    QString error;
    double last_progress = 0.0;
    assert(ui::ProjectExporter::render_wav(
        *plan, events, 120.0, 0.0f, time::BeatPosition::zero(),
        time::BeatPosition::from_beats(4), path, nullptr,
        [&last_progress](double progress) { last_progress = progress; }, &error));
    assert(error.isEmpty());
    assert(last_progress == 1.0);

    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    assert(bytes.size() == 44 + 96000 * 2 * 2);
    assert(std::memcmp(bytes.constData(), "RIFF", 4) == 0);
    assert(std::memcmp(bytes.constData() + 8, "WAVE", 4) == 0);
    assert(le32(bytes.constData() + 24) == 48000);
    assert(le32(bytes.constData() + 40) == 96000U * 4U);

    bool non_zero = false;
    for (qsizetype i = 44; i < bytes.size(); ++i) {
        if (bytes[i] != 0) {
            non_zero = true;
            break;
        }
    }
    assert(non_zero);
    std::cout << "OFFLINE WAV EXPORT PASSED\n";
    return 0;
}
