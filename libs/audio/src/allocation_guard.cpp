#include <saudade/audio/allocation_guard.hpp>
#include <cstdlib>
#include <new>

namespace saudade::audio {

namespace {
thread_local bool t_realtime_scope_active = false;
thread_local size_t t_realtime_allocation_count = 0;
} // namespace

bool is_realtime_scope_active() noexcept {
    return t_realtime_scope_active;
}

size_t get_realtime_allocation_count() noexcept {
    return t_realtime_allocation_count;
}

void reset_realtime_allocation_count() noexcept {
    t_realtime_allocation_count = 0;
}

void record_realtime_violation() noexcept {
    if (t_realtime_scope_active) {
        ++t_realtime_allocation_count;
    }
}

ScopedRealtimeGuard::ScopedRealtimeGuard() noexcept
    : previous_state_(t_realtime_scope_active) {
    t_realtime_scope_active = true;
}

ScopedRealtimeGuard::~ScopedRealtimeGuard() {
    t_realtime_scope_active = previous_state_;
}

} // namespace saudade::audio

// Intercept global C++ allocations and deallocations
void* operator new(std::size_t size) {
    saudade::audio::record_realtime_violation();
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](std::size_t size) {
    saudade::audio::record_realtime_violation();
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    saudade::audio::record_realtime_violation();
    return std::malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    saudade::audio::record_realtime_violation();
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    saudade::audio::record_realtime_violation();
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    saudade::audio::record_realtime_violation();
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    saudade::audio::record_realtime_violation();
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    saudade::audio::record_realtime_violation();
    std::free(ptr);
}
