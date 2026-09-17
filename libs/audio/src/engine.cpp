#include <saudade/audio/engine.hpp>
#include <saudade/audio/allocation_guard.hpp>
#include <cassert>
#include <thread>

namespace saudade::audio {

AudioEngine::AudioEngine(std::unique_ptr<const renderplan::RenderPlan> initial_plan,
                         uint32_t max_block_size)
    : max_block_size_(max_block_size) {
    assert(initial_plan != nullptr);
    publisher_.publish(std::move(initial_plan), max_block_size_);
}

void AudioEngine::prepare(uint32_t max_block_size) {
    max_block_size_ = max_block_size;
    publisher_.prepare(max_block_size_);
}

void AudioEngine::reset() {
    publisher_.reset_active_dsp_state();
    event_queue_.clear();
    event_queue_.reset_schedule_tracking(0);
    const uint64_t req = flush_requested_.load(std::memory_order_relaxed);
    flush_acknowledged_.store(req, std::memory_order_relaxed);
}

PlanGeneration AudioEngine::publish_plan(std::unique_ptr<const renderplan::RenderPlan> new_plan) {
    return publisher_.publish(std::move(new_plan), max_block_size_);
}

size_t AudioEngine::collect_retired() {
    return publisher_.collect_retired();
}

uint64_t AudioEngine::request_event_flush() noexcept {
    const uint64_t gen = ++next_flush_request_;
    flush_requested_.store(gen, std::memory_order_release);
    return gen;
}

bool AudioEngine::is_flush_acknowledged(uint64_t generation) const noexcept {
    return flush_acknowledged_.load(std::memory_order_acquire) >= generation;
}

bool AudioEngine::wait_for_flush(uint64_t generation, std::chrono::milliseconds timeout) noexcept {
    const auto start = std::chrono::steady_clock::now();
    while (!is_flush_acknowledged(generation)) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start) > timeout) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

bool AudioEngine::flush_events(std::chrono::milliseconds timeout) noexcept {
    const uint64_t gen = request_event_flush();
    if (!wait_for_flush(gen, timeout)) {
        return false;
    }
    event_queue_.reset_schedule_tracking(0);
    return true;
}

void AudioEngine::process(AudioBlock& output_block, const ProcessContext& ctx) noexcept {
    // Realtime allocation guard active during execution
    ScopedRealtimeGuard rt_guard;

    if (ctx.num_frames == 0 || ctx.num_frames > max_block_size_) {
        return;
    }

    // Capture transport state snapshot for this quantum
    const time::TransportSnapshot transport_snap = transport_.acquire_snapshot();

    // Law 3: Snapshot active plan EXACTLY ONCE per audio quantum
    PreparedPlan* const current = publisher_.acquire_current_for_quantum();

    // RT-Safe Event Flush Handshake:
    // If control thread requested a flush, drain the queue and reset voices now.
    const uint64_t req = flush_requested_.load(std::memory_order_acquire);
    const uint64_t ack = flush_acknowledged_.load(std::memory_order_relaxed);
    if (req > ack) {
        while (event_queue_.pop()) {}
        quantum_event_block_.clear();
        if (current) {
            current->dsp_state.reset();
        }
        flush_acknowledged_.store(req, std::memory_order_release);
    }

    if (!current || !transport_snap.playing) {
        output_block.clear();
        if (current) {
            publisher_.acknowledge_completed_generation(current->generation);
        }
        return;
    }

    // Populate quantum context with transport snapshot data
    ProcessContext local_ctx = ctx;
    local_ctx.block_start_sample = transport_snap.block_start_sample;
    local_ctx.transport_playing = true;

    // Drain events for this quantum: [block_start_sample, block_start_sample + num_frames)
    quantum_event_block_.clear();
    const time::SamplePosition block_end = transport_snap.block_start_sample + static_cast<time::SamplePosition>(ctx.num_frames);

    events::TimelineEvent ev{};
    while (event_queue_.peek(ev)) {
        if (ev.sample_position < transport_snap.block_start_sample) {
            // Discard stale event (e.g. after seek forward)
            event_queue_.pop();
            continue;
        }
        if (ev.sample_position >= block_end) {
            // Future event belongs to a later quantum; wait in queue
            break;
        }
        // Event belongs to current quantum
        event_queue_.pop();
        const uint32_t offset = static_cast<uint32_t>(ev.sample_position - transport_snap.block_start_sample);
        quantum_event_block_.push_back(events::TimedEvent(offset, ev.payload));
    }

    // Deterministic in-place sort without allocation
    quantum_event_block_.sort();

    // Execute entire quantum with the snapshotted plan, preallocated buffers, and quantum events
    current->plan->render(output_block, current->scratch_buffers, current->dsp_state, local_ctx, quantum_event_block_.view());

    // Law 6: Acknowledge generation completion for safe non-RT reclamation
    publisher_.acknowledge_completed_generation(current->generation);

    // Advance transport timeline position by processed frames
    transport_.advance_quantum(ctx.num_frames);
}

} // namespace saudade::audio
