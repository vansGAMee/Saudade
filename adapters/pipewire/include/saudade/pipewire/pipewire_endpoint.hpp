#pragma once

#include <saudade/audio/endpoint.hpp>
#include <saudade/audio/engine.hpp>

#include <memory>
#include <string>
#include <atomic>
#include <thread>

// Forward declarations of opaque PipeWire/C types.
// DO NOT include pipewire headers in this header file to prevent leaking PipeWire types into consumers.
struct pw_main_loop;
struct pw_filter;
struct pw_core;
struct pw_registry;
struct spa_hook;
struct spa_io_position;

namespace saudade::pipewire {

/// Boundary adapter connecting Saudade AudioEngine to the Linux PipeWire audio system.
/// Core engine and renderplan have zero knowledge of this adapter.
class PipeWireEndpoint : public audio::IAudioEndpoint {
public:
    explicit PipeWireEndpoint(audio::AudioEngine& engine,
                              std::string app_name = "saudade-audio-proof");
    ~PipeWireEndpoint() override;

    PipeWireEndpoint(const PipeWireEndpoint&) = delete;
    PipeWireEndpoint& operator=(const PipeWireEndpoint&) = delete;

    void start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const noexcept override;

    [[nodiscard]] double sample_rate() const noexcept override;
    [[nodiscard]] uint32_t quantum() const noexcept override;

    /// Waits until at least one realtime audio process callback has executed, or timeout.
    bool wait_for_stream(uint32_t timeout_ms = 3000);

    void notify_state_changed();

private:
    void loop_thread_fn();
    static void on_process_thunk(void* userdata, struct spa_io_position* position);
    void handle_process(struct spa_io_position* position) noexcept;

    audio::AudioEngine& engine_;
    std::string app_name_;

    std::atomic<bool> running_{false};
    std::atomic<bool> streaming_{false};
    std::atomic<double> sample_rate_{48000.0};
    std::atomic<uint32_t> quantum_{512};
    std::atomic<uint64_t> process_count_{0};

    // Preallocated channel pointers for realtime callback (zero-alloc)
    float* channel_ptrs_[2]{nullptr, nullptr};

    struct pw_main_loop* loop_{nullptr};
    struct pw_filter* filter_{nullptr};
    void* port_left_{nullptr};
    void* port_right_{nullptr};
    void* reg_ctx_{nullptr};

    std::thread loop_thread_;
};

} // namespace saudade::pipewire
