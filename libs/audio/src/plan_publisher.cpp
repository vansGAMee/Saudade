#include <saudade/audio/plan_publisher.hpp>
#include <stdexcept>

namespace saudade::audio {

PlanPublisher::PlanPublisher() {
    static_assert(decltype(active_plan_)::is_always_lock_free,
                  "active_plan_ must be lock-free on this platform");
    static_assert(decltype(completed_generation_)::is_always_lock_free,
                  "completed_generation_ must be lock-free on this platform");
}

PlanPublisher::~PlanPublisher() {
    // Realtime callback must already be stopped prior to destruction.
    active_plan_.store(nullptr, std::memory_order_release);
    active_holder_.reset();
    retired_plans_.clear();
}

PlanGeneration PlanPublisher::publish(std::unique_ptr<const renderplan::RenderPlan> plan,
                                      uint32_t max_block_size) {
    if (!plan) {
        throw std::invalid_argument("Cannot publish null RenderPlan");
    }

    // 1. Fully construct the PreparedPlan outside the lock and outside realtime.
    auto new_prepared = std::make_unique<PreparedPlan>();
    new_prepared->dsp_state = plan->create_dsp_state();
    new_prepared->scratch_buffers.resize(plan->num_scratch_buffers(), max_block_size);
    new_prepared->plan = std::move(plan);

    std::lock_guard<std::mutex> lock(control_mutex_);

    const PlanGeneration gen = ++next_generation_;
    new_prepared->generation = gen;
    PreparedPlan* const raw_ptr = new_prepared.get();

    // 2. Retire the previously active plan.
    // The previous plan can only be reclaimed once the RT thread has completed a quantum
    // executing generation >= gen (which replaces it).
    if (active_holder_) {
        active_holder_->retire_barrier_generation = gen;
        retired_plans_.push_back(std::move(active_holder_));
    }

    // 3. Keep ownership of the newly active plan on the control side.
    active_holder_ = std::move(new_prepared);

    // 4. Release-store publishes the pointer to the realtime thread.
    // Happens-before logic:
    // Memory order release ensures that all previous writes (allocations, DSP state init,
    // buffer sizing) are committed and visible to any thread performing an acquire load
    // on active_plan_.
    active_plan_.store(raw_ptr, std::memory_order_release);

    return gen;
}

void PlanPublisher::prepare(uint32_t max_block_size) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (active_holder_ && active_holder_->plan) {
        active_holder_->scratch_buffers.resize(active_holder_->plan->num_scratch_buffers(), max_block_size);
    }
}

size_t PlanPublisher::collect_retired() {
    std::lock_guard<std::mutex> lock(control_mutex_);

    // Memory order acquire synchronizes with the release-store in acknowledge_completed_generation.
    // All DSP state mutations and buffer reads performed by the RT thread on the retired plan
    // happen-before this load and any subsequent deletion.
    const PlanGeneration ack = completed_generation_.load(std::memory_order_acquire);

    size_t reclaimed = 0;
    auto it = retired_plans_.begin();
    while (it != retired_plans_.end()) {
        if ((*it)->retire_barrier_generation <= ack) {
            it = retired_plans_.erase(it);
            ++reclaimed;
        } else {
            ++it;
        }
    }
    return reclaimed;
}

size_t PlanPublisher::retired_count() const {
    std::lock_guard<std::mutex> lock(control_mutex_);
    return retired_plans_.size();
}

PlanGeneration PlanPublisher::active_generation() const {
    std::lock_guard<std::mutex> lock(control_mutex_);
    return active_holder_ ? active_holder_->generation : 0;
}

PlanGeneration PlanPublisher::completed_generation() const noexcept {
    return completed_generation_.load(std::memory_order_acquire);
}

bool PlanPublisher::has_active_plan() const noexcept {
    return active_plan_.load(std::memory_order_acquire) != nullptr;
}

const renderplan::RenderPlan& PlanPublisher::active_plan() const {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (!active_holder_ || !active_holder_->plan) {
        throw std::runtime_error("No active RenderPlan published");
    }
    return *active_holder_->plan;
}

void PlanPublisher::reset_active_dsp_state() {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (active_holder_) {
        active_holder_->dsp_state.reset();
    }
}

PreparedPlan* PlanPublisher::acquire_current_for_quantum() noexcept {
    // Memory order acquire synchronizes with the release-store in publish().
    // Ensures all plan and buffer initialization is visible to the RT thread.
    return active_plan_.load(std::memory_order_acquire);
}

void PlanPublisher::acknowledge_completed_generation(PlanGeneration gen) noexcept {
    // Memory order release ensures that all RT reads and writes during the quantum
    // happen-before completed_generation_ is updated.
    completed_generation_.store(gen, std::memory_order_release);
}

} // namespace saudade::audio
