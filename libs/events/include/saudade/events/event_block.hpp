#pragma once

#include <saudade/events/event_types.hpp>

#include <span>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace saudade::events {

/// Non-owning, immutable view of contiguous TimedEvents for the active quantum.
class EventBlockView {
public:
    constexpr EventBlockView() noexcept = default;

    constexpr EventBlockView(const TimedEvent* data, size_t size) noexcept
        : span_(data, size) {}

    constexpr EventBlockView(std::span<const TimedEvent> span) noexcept
        : span_(span) {}

    [[nodiscard]] constexpr const TimedEvent* data() const noexcept { return span_.data(); }
    [[nodiscard]] constexpr size_t size() const noexcept { return span_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return span_.empty(); }

    [[nodiscard]] constexpr const TimedEvent& operator[](size_t index) const noexcept {
        return span_[index];
    }

    [[nodiscard]] constexpr auto begin() const noexcept { return span_.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return span_.end(); }

private:
    std::span<const TimedEvent> span_{};
};

/// Fixed-capacity, preallocated event block container for real-time quantum execution.
/// Guarantees zero heap allocations during realtime processing.
/// Hard overflow policy: excess events are dropped with an observable counter.
template <size_t MaxCapacity = 512>
class FixedEventBlock {
public:
    static constexpr size_t kCapacity = MaxCapacity;

    FixedEventBlock() noexcept = default;

    /// Appends a TimedEvent if capacity allows; drops and increments counter if full.
    bool push_back(const TimedEvent& event) noexcept {
        if (size_ >= kCapacity) {
            ++drop_count_;
            return false;
        }
        events_[size_++] = event;
        return true;
    }

    /// Clears the block without any deallocations.
    void clear() noexcept {
        size_ = 0;
    }

    /// Sorts events in-place according to deterministic operator<.
    void sort() noexcept {
        if (size_ > 1) {
            std::sort(events_.begin(), events_.begin() + static_cast<ptrdiff_t>(size_));
        }
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_t capacity() const noexcept { return kCapacity; }
    [[nodiscard]] uint64_t drop_count() const noexcept { return drop_count_; }
    void reset_drop_count() noexcept { drop_count_ = 0; }

    [[nodiscard]] const TimedEvent& operator[](size_t index) const noexcept {
        return events_[index];
    }

    [[nodiscard]] EventBlockView view() const noexcept {
        return EventBlockView(events_.data(), size_);
    }

private:
    std::array<TimedEvent, kCapacity> events_{};
    size_t size_{0};
    uint64_t drop_count_{0};
};

using EventBlock = FixedEventBlock<512>;

} // namespace saudade::events
