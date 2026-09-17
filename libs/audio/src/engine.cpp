#include <saudade/audio/engine.hpp>
#include <saudade/audio/allocation_guard.hpp>
#include <cassert>

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
}

PlanGeneration AudioEngine::publish_plan(std::unique_ptr<const renderplan::RenderPlan> new_plan) {
    return publisher_.publish(std::move(new_plan), max_block_size_);
}

size_t AudioEngine::collect_retired() {
    return publisher_.collect_retired();
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

    // Execute entire quantum with the snapshotted plan and its preallocated buffers
    current->plan->render(output_block, current->scratch_buffers, current->dsp_state, local_ctx);

    // Law 6: Acknowledge generation completion for safe non-RT reclamation
    publisher_.acknowledge_completed_generation(current->generation);

    // Advance transport timeline position by processed frames
    transport_.advance_quantum(ctx.num_frames);
}

} // namespace saudade::audio
