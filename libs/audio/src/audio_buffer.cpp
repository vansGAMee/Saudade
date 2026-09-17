#include <saudade/audio/audio_buffer.hpp>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>

namespace saudade::audio {

namespace {

size_t aligned_channel_stride(uint32_t capacity_frames) noexcept {
    // 64-byte alignment requires multiple of 16 float samples
    constexpr size_t kSamplesPerAlignment = AudioBuffer::kAlignmentBytes / sizeof(float);
    const size_t frames = static_cast<size_t>(capacity_frames);
    return ((frames + kSamplesPerAlignment - 1) / kSamplesPerAlignment) * kSamplesPerAlignment;
}

} // namespace

AudioBuffer::AudioBuffer(size_t num_channels, uint32_t capacity_frames)
    : num_channels_(num_channels), capacity_frames_(capacity_frames) {
    allocate();
}

AudioBuffer::~AudioBuffer() {
    release();
}

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : num_channels_(other.num_channels_),
      capacity_frames_(other.capacity_frames_),
      memory_(other.memory_),
      channel_ptrs_(std::move(other.channel_ptrs_)) {
    other.num_channels_ = 0;
    other.capacity_frames_ = 0;
    other.memory_ = nullptr;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        release();
        num_channels_ = other.num_channels_;
        capacity_frames_ = other.capacity_frames_;
        memory_ = other.memory_;
        channel_ptrs_ = std::move(other.channel_ptrs_);

        other.num_channels_ = 0;
        other.capacity_frames_ = 0;
        other.memory_ = nullptr;
    }
    return *this;
}

void AudioBuffer::resize(size_t num_channels, uint32_t capacity_frames) {
    if (num_channels_ == num_channels && capacity_frames_ >= capacity_frames) {
        // Already sufficient capacity
        return;
    }
    release();
    num_channels_ = num_channels;
    capacity_frames_ = capacity_frames;
    allocate();
}

void AudioBuffer::allocate() {
    if (num_channels_ == 0 || capacity_frames_ == 0) {
        memory_ = nullptr;
        channel_ptrs_.clear();
        return;
    }

    const size_t stride = aligned_channel_stride(capacity_frames_);
    const size_t total_floats = num_channels_ * stride;
    const size_t total_bytes = total_floats * sizeof(float);

    // Allocate aligned memory outside realtime thread
    void* raw_mem = nullptr;
    const int res = posix_memalign(&raw_mem, kAlignmentBytes, total_bytes);
    if (res != 0 || !raw_mem) {
        throw std::bad_alloc();
    }

    memory_ = static_cast<float*>(raw_mem);
    std::memset(memory_, 0, total_bytes);

    channel_ptrs_.resize(num_channels_);
    for (size_t ch = 0; ch < num_channels_; ++ch) {
        channel_ptrs_[ch] = memory_ + (ch * stride);
    }
}

void AudioBuffer::release() noexcept {
    if (memory_) {
        std::free(memory_);
        memory_ = nullptr;
    }
    channel_ptrs_.clear();
    num_channels_ = 0;
    capacity_frames_ = 0;
}

AudioBlock AudioBuffer::block(uint32_t num_frames) noexcept {
    assert(num_frames <= capacity_frames_);
    return AudioBlock(channel_ptrs_.data(), num_channels_, num_frames);
}

AudioBlock AudioBuffer::block(uint32_t num_frames) const noexcept {
    assert(num_frames <= capacity_frames_);
    return AudioBlock(channel_ptrs_.data(), num_channels_, num_frames);
}

void AudioBuffer::clear(uint32_t num_frames) noexcept {
    const uint32_t frames_to_clear = std::min(num_frames, capacity_frames_);
    if (frames_to_clear == 0) {
        return;
    }
    for (size_t ch = 0; ch < num_channels_; ++ch) {
        std::memset(channel_ptrs_[ch], 0, frames_to_clear * sizeof(float));
    }
}

} // namespace saudade::audio
