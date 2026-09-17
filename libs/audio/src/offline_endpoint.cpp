#include <saudade/audio/offline_endpoint.hpp>
#include <cassert>

namespace saudade::audio {

OfflineEndpoint::OfflineEndpoint(AudioEngine& engine,
                                 double sample_rate,
                                 uint32_t quantum,
                                 size_t num_channels)
    : engine_(engine),
      sample_rate_(sample_rate),
      quantum_(quantum),
      num_channels_(num_channels),
      block_buffer_(num_channels, quantum) {
    recorded_channels_.resize(num_channels_);
}

void OfflineEndpoint::start() {
    running_ = true;
    if (!engine_.transport().is_playing()) {
        engine_.transport().play();
    }
}

void OfflineEndpoint::stop() {
    running_ = false;
    engine_.transport().stop();
}

void OfflineEndpoint::reset() {
    for (auto& ch : recorded_channels_) {
        ch.clear();
    }
    engine_.reset();
    engine_.transport().seek_samples(0);
}

void OfflineEndpoint::render_blocks(size_t num_blocks) {
    if (!running_) {
        start();
    }

    // Allocate storage for recorded audio outside the DSP execution
    for (auto& ch : recorded_channels_) {
        ch.reserve(ch.size() + num_blocks * quantum_);
    }

    ProcessContext ctx{sample_rate_, quantum_};

    for (size_t b = 0; b < num_blocks; ++b) {
        block_buffer_.clear(quantum_);
        AudioBlock block = block_buffer_.block(quantum_);
        engine_.process(block, ctx);

        for (size_t ch = 0; ch < num_channels_; ++ch) {
            const float* src = block.channel(ch);
            recorded_channels_[ch].insert(recorded_channels_[ch].end(), src, src + quantum_);
        }
    }
}

void OfflineEndpoint::render_frames(uint32_t total_frames) {
    if (quantum_ == 0) {
        return;
    }
    const size_t num_blocks = (total_frames + quantum_ - 1) / quantum_;
    render_blocks(num_blocks);
}

} // namespace saudade::audio
