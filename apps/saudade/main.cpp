#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/pipewire/pipewire_endpoint.hpp>
#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/piano_roll_item.hpp>
#include <saudade/ui/piano_keys_item.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
#include <QUrl>

#include <iostream>
#include <memory>
#include <csignal>

namespace {

std::unique_ptr<saudade::renderplan::RenderPlan> create_synth_plan() {
    saudade::graph::GraphModel graph;
    const auto synth_node = graph.add_poly_synth_node();
    const auto gain_left = graph.add_gain_node(-9.0f);
    const auto gain_right = graph.add_gain_node(-9.0f);
    const auto out_node = graph.add_output_node(2);

    graph.connect(synth_node, saudade::graph::PolySynthNode::kPortLeft,
                  gain_left, saudade::graph::GainNode::kPortIn);
    graph.connect(synth_node, saudade::graph::PolySynthNode::kPortRight,
                  gain_right, saudade::graph::GainNode::kPortIn);
    graph.connect(gain_left, saudade::graph::GainNode::kPortOut,
                  out_node, saudade::graph::OutputNode::kPortLeft);
    graph.connect(gain_right, saudade::graph::GainNode::kPortOut,
                  out_node, saudade::graph::OutputNode::kPortRight);

    return saudade::graph::GraphCompiler::compile(graph);
}

} // namespace

int main(int argc, char* argv[]) {
    // 1. Initialize Audio Engine & RenderPlan
    auto plan = create_synth_plan();
    saudade::audio::AudioEngine engine(std::move(plan));

    // 2. Start PipeWire boundary endpoint
    saudade::pipewire::PipeWireEndpoint endpoint(engine, "saudade");
    endpoint.start();

    // Stream negotiation wait (up to 1500ms)
    if (!endpoint.wait_for_stream(1500)) {
        std::cerr << "Note: Audio stream initializing in background...\n";
    }

    const double sample_rate = endpoint.sample_rate() > 0 ? endpoint.sample_rate() : 48000.0;

    // 3. Initialize Qt Application
    QGuiApplication app(argc, argv);
    app.setApplicationName("Saudade");
    app.setOrganizationName("SaudadeDAW");

    // 4. Register custom C++ Scene Graph items
    qmlRegisterType<saudade::ui::PianoRollItem>("saudade.ui", 1, 0, "PianoRollItem");
    qmlRegisterType<saudade::ui::PianoKeysItem>("saudade.ui", 1, 0, "PianoKeysItem");
    qmlRegisterUncreatableType<saudade::ui::EditorController>("saudade.ui", 1, 0, "EditorController", "C++ only");

    // 5. Create EditorController connecting model and audio engine
    saudade::ui::EditorController controller(engine, sample_rate);

    // 6. Initialize QML Application Engine
    QQmlApplicationEngine qml_engine;
    qml_engine.rootContext()->setContextProperty("editorController", &controller);
    qml_engine.rootContext()->setContextProperty("controller", &controller);

    QUrl qml_url(QStringLiteral("qrc:/saudade/Main.qml"));
    if (!QFile::exists(":/saudade/Main.qml")) {
#ifdef SAUDADE_QML_SOURCE_DIR
        qml_url = QUrl::fromLocalFile(QStringLiteral(SAUDADE_QML_SOURCE_DIR "/Main.qml"));
#endif
    }

    QObject::connect(
        &qml_engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    qml_engine.load(qml_url);

    if (qml_engine.rootObjects().isEmpty()) {
        std::cerr << "Failed to load QML interface from " << qml_url.toString().toStdString() << "\n";
        endpoint.stop();
        return -1;
    }

    // 7. Run GUI event loop
    const int ret = app.exec();

    // 8. Graceful audio endpoint shutdown
    endpoint.stop();
    engine.collect_retired();

    return ret;
}
