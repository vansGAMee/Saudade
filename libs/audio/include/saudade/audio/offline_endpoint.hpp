#pragma once

#include <saudade/audio/endpoint.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/audio/audio_buffer.hpp>

#include <vector>
#include <cstdint>
#include <cstddef>

namespace saudade::audio {

/// Offline endpoint that renders an AudioEngine into memory buffers for tests.
class OfflineEndpoint : public IAudioEndpoint {
public:
    OfflineEndpoint(AudioEngine& engine,
                    double sample_rate = 48000.0,
                    uint32_t quantum = 512,
                    size_t num_channels = 2);

    void start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const noexcept override { return running_; }

    [[nodiscard]] double sample_rate() const noexcept override { return sample_rate_; }
    [[nodiscard]] uint32_t quantum() const noexcept override { return quantum_; }
    [[nodiscard]] size_t num_channels() const noexcept { return num_channels_; }

    /// Renders a given number of blocks offline.
    void render_blocks(size_t num_blocks);

    /// Renders at least the specified total number of frames.
    void render_frames(uint32_t total_frames);

    /// Clears recorded audio data and resets engine state.
    void reset();

    /// Returns recorded planar channel data.
    [[nodiscard]] const std::vector<std::vector<float>>& recorded_channels() const noexcept {
        return recorded_channels_;
    }

    [[nodiscard]] size_t total_rendered_frames() const noexcept {
        return recorded_channels_.empty() ? 0 : recorded_channels_[0].size();
    }

private:
    AudioEngine& engine_;
    double sample_rate_{48000.0};
    uint32_t quantum_{512};
    size_t num_channels_{2};
    bool running_{false};

    AudioBuffer block_buffer_;
    std::vector<std::vector<float>> recorded_channels_;
};

} // namespace saudade::audio
