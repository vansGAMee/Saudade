#pragma once

#include <cstddef>

namespace saudade::audio {

/// Checks if the current thread is executing within a protected realtime scope.
[[nodiscard]] bool is_realtime_scope_active() noexcept;

/// Returns the number of memory allocation violations recorded in realtime scopes on this thread.
[[nodiscard]] size_t get_realtime_allocation_count() noexcept;

/// Resets the count of recorded realtime allocation violations for this thread.
void reset_realtime_allocation_count() noexcept;

/// Records a violation if called while inside an active realtime scope.
void record_realtime_violation() noexcept;

/// RAII scope guard to instrument realtime execution blocks.
/// Any dynamic memory allocation during the lifetime of this object will be detected.
class ScopedRealtimeGuard {
public:
    ScopedRealtimeGuard() noexcept;
    ~ScopedRealtimeGuard();

    ScopedRealtimeGuard(const ScopedRealtimeGuard&) = delete;
    ScopedRealtimeGuard& operator=(const ScopedRealtimeGuard&) = delete;

private:
    bool previous_state_{false};
};

} // namespace saudade::audio
