#pragma once

#include <cstdint>

namespace saudade::audio {

/// Common interface for audio endpoints (Offline, PipeWire, etc.).
class IAudioEndpoint {
public:
    virtual ~IAudioEndpoint() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_running() const noexcept = 0;

    [[nodiscard]] virtual double sample_rate() const noexcept = 0;
    [[nodiscard]] virtual uint32_t quantum() const noexcept = 0;
};

} // namespace saudade::audio
