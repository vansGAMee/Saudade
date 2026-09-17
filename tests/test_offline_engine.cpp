#include <saudade/graph/graph_model.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/offline_endpoint.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <memory>

namespace {

std::unique_ptr<saudade::renderplan::RenderPlan> make_sine_plan() {
    saudade::graph::GraphModel graph;
    const auto sine = graph.add_sine_node(440.0f);
    const auto gain = graph.add_gain_node(-12.0f);
    const auto out = graph.add_output_node(2);

    graph.connect(sine, saudade::graph::SineNode::kPortOut,
                  gain, saudade::graph::GainNode::kPortIn);
    graph.connect(gain, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortLeft);
    graph.connect(gain, saudade::graph::GainNode::kPortOut,
                  out, saudade::graph::OutputNode::kPortRight);

    return saudade::graph::GraphCompiler::compile(graph);
}

} // namespace

void test_offline_render_properties() {
    std::cout << "[RUN] test_offline_render_properties\n";

    auto plan = make_sine_plan();
    saudade::audio::AudioEngine engine(std::move(plan));

    const double sample_rate = 48000.0;
    const uint32_t quantum = 256;
    saudade::audio::OfflineEndpoint endpoint(engine, sample_rate, quantum, 2);

    // Render 1 second (48000 frames = 187.5 blocks -> render 188 blocks = 48128 frames)
    const size_t num_blocks = 188;
    endpoint.render_blocks(num_blocks);

    const auto& channels = endpoint.recorded_channels();

    // A. Correct buffer length
    assert(channels.size() == 2);
    const size_t total_frames = num_blocks * quantum;
    assert(channels[0].size() == total_frames);
    assert(channels[1].size() == total_frames);
    std::cout << "  [PASS] A. Correct buffer length: " << total_frames << " frames\n";

    // B. Finite values (no NaN, no Inf)
    for (size_t ch = 0; ch < channels.size(); ++ch) {
        for (size_t i = 0; i < total_frames; ++i) {
            const float sample = channels[ch][i];
            assert(std::isfinite(sample));
        }
    }
    std::cout << "  [PASS] B. Finite values (no NaN or Inf)\n";

    // C. Frequency check (~440 Hz)
    // Count positive zero crossings in the first 48000 frames (1 second)
    const size_t frames_1s = 48000;
    int zero_crossings = 0;
    for (size_t i = 1; i < frames_1s; ++i) {
        if (channels[0][i - 1] <= 0.0f && channels[0][i] > 0.0f) {
            ++zero_crossings;
        }
    }
    // At 440 Hz for 1 second, expected positive zero-crossings is 440 (allow +/- 2 due to boundary phase)
    assert(std::abs(zero_crossings - 440) <= 2);
    std::cout << "  [PASS] C. Frequency verification: " << zero_crossings << " cycles in 1 sec (~440 Hz)\n";

    // D. Gain check (~ -12 dB = 0.25118864 peak amplitude)
    const float expected_peak = std::pow(10.0f, -12.0f / 20.0f); // ~0.25118864
    float max_peak_l = 0.0f;
    float max_peak_r = 0.0f;
    for (size_t i = 0; i < total_frames; ++i) {
        max_peak_l = std::max(max_peak_l, std::abs(channels[0][i]));
        max_peak_r = std::max(max_peak_r, std::abs(channels[1][i]));
    }
    assert(std::abs(max_peak_l - expected_peak) < 0.005f);
    assert(std::abs(max_peak_r - expected_peak) < 0.005f);
    std::cout << "  [PASS] D. Gain verification: peak amplitude = " << max_peak_l
              << " (expected ~" << expected_peak << ")\n";

    // Left and Right channels must be identical for stereo proof
    for (size_t i = 0; i < total_frames; ++i) {
        assert(channels[0][i] == channels[1][i]);
    }
    std::cout << "  [PASS] Stereo consistency: Left == Right\n";
}

void test_block_continuity() {
    std::cout << "[RUN] test_block_continuity\n";

    const double sample_rate = 48000.0;

    // Run 1: render two 128-frame blocks (128 + 128 = 256 frames total)
    auto plan_blocks = make_sine_plan();
    saudade::audio::AudioEngine engine_blocks(std::move(plan_blocks));
    saudade::audio::OfflineEndpoint ep_blocks(engine_blocks, sample_rate, 128, 2);
    ep_blocks.render_blocks(2);

    // Run 2: render one 256-frame block
    auto plan_continuous = make_sine_plan();
    saudade::audio::AudioEngine engine_continuous(std::move(plan_continuous));
    saudade::audio::OfflineEndpoint ep_continuous(engine_continuous, sample_rate, 256, 2);
    ep_continuous.render_blocks(1);

    const auto& samples_blocks = ep_blocks.recorded_channels()[0];
    const auto& samples_continuous = ep_continuous.recorded_channels()[0];

    assert(samples_blocks.size() == 256);
    assert(samples_continuous.size() == 256);

    for (size_t i = 0; i < 256; ++i) {
        const float diff = std::abs(samples_blocks[i] - samples_continuous[i]);
        assert(diff < 1e-5f);
    }
    std::cout << "  [PASS] E. Phase continuity across blocks: 2x128 blocks matches continuous 1x256 block\n";
}

int main() {
    try {
        test_offline_render_properties();
        test_block_continuity();
        std::cout << "All offline engine tests PASSED!\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
