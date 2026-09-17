#pragma once

#include <saudade/audio/audio_block.hpp>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>

namespace saudade::audio {

/// Owning planar float32 audio buffer with aligned channel memory.
/// Must be allocated and resized strictly outside the realtime thread.
class AudioBuffer {
public:
    static constexpr size_t kAlignmentBytes = 64;

    AudioBuffer() noexcept = default;
    AudioBuffer(size_t num_channels, uint32_t capacity_frames);
    ~AudioBuffer();

    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;

    AudioBuffer(AudioBuffer&& other) noexcept;
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;

    void resize(size_t num_channels, uint32_t capacity_frames);

    [[nodiscard]] size_t num_channels() const noexcept { return num_channels_; }
    [[nodiscard]] uint32_t capacity_frames() const noexcept { return capacity_frames_; }

    [[nodiscard]] float* channel(size_t ch) noexcept {
        assert(ch < num_channels_);
        return channel_ptrs_[ch];
    }

    [[nodiscard]] const float* channel(size_t ch) const noexcept {
        assert(ch < num_channels_);
        return channel_ptrs_[ch];
    }

    [[nodiscard]] float* const* channel_pointers() noexcept {
        return channel_ptrs_.data();
    }

    [[nodiscard]] float* const* channel_pointers() const noexcept {
        return channel_ptrs_.data();
    }

    /// Returns a non-owning AudioBlock view of the first num_frames.
    [[nodiscard]] AudioBlock block(uint32_t num_frames) noexcept;
    [[nodiscard]] AudioBlock block(uint32_t num_frames) const noexcept;

    /// Realtime-safe zero-fill of the channels up to num_frames.
    void clear(uint32_t num_frames) noexcept;

private:
    void allocate();
    void release() noexcept;

    size_t num_channels_{0};
    uint32_t capacity_frames_{0};
    float* memory_{nullptr};
    std::vector<float*> channel_ptrs_;
};

} // namespace saudade::audio
