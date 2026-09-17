#pragma once

#include <span>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>

namespace saudade::audio {

/// Non-owning view of planar float32 audio channels for a single block.
/// Used in the realtime render path without any dynamic memory allocations.
class AudioBlock {
public:
    constexpr AudioBlock() noexcept = default;

    constexpr AudioBlock(float* const* channels, size_t num_channels, uint32_t num_frames) noexcept
        : channels_(channels), num_channels_(num_channels), num_frames_(num_frames) {}

    [[nodiscard]] constexpr size_t num_channels() const noexcept { return num_channels_; }
    [[nodiscard]] constexpr uint32_t num_frames() const noexcept { return num_frames_; }

    [[nodiscard]] constexpr float* channel(size_t ch) const noexcept {
        assert(ch < num_channels_);
        return channels_[ch];
    }

    [[nodiscard]] constexpr std::span<float> channel_span(size_t ch) const noexcept {
        assert(ch < num_channels_);
        return {channels_[ch], num_frames_};
    }

    [[nodiscard]] constexpr float* const* data() const noexcept { return channels_; }

    /// Zeros all frames across all channels without dynamic memory allocations.
    void clear() const noexcept {
        for (size_t ch = 0; ch < num_channels_; ++ch) {
            if (channels_[ch] != nullptr) {
                std::memset(channels_[ch], 0, num_frames_ * sizeof(float));
            }
        }
    }

private:
    float* const* channels_{nullptr};
    size_t num_channels_{0};
    uint32_t num_frames_{0};
};

} // namespace saudade::audio
