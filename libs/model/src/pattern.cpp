#include <saudade/model/pattern.hpp>
#include <algorithm>

namespace saudade::model {

Pattern::Pattern(PatternId id, std::string name, time::BeatDuration length) noexcept
    : id_(id), name_(std::move(name)), length_(length) {
    if (length_.ticks <= 0) {
        length_ = time::BeatDuration::from_beats(4);
    }
}

bool Pattern::set_length(time::BeatDuration length) noexcept {
    if (length.ticks <= 0) {
        return false;
    }
    length_ = length;
    return true;
}

LaneId Pattern::add_lane(std::string name) {
    LaneId id = next_lane_id_++;
    lanes_.emplace_back(id, std::move(name));
    return id;
}

bool Pattern::remove_lane(LaneId id) noexcept {
    const auto it = std::find_if(lanes_.begin(), lanes_.end(), [id](const PatternLane& lane) {
        return lane.id() == id;
    });
    if (it == lanes_.end()) {
        return false;
    }
    lanes_.erase(it);
    return true;
}

PatternLane* Pattern::find_lane(LaneId id) noexcept {
    for (auto& lane : lanes_) {
        if (lane.id() == id) {
            return &lane;
        }
    }
    return nullptr;
}

const PatternLane* Pattern::find_lane(LaneId id) const noexcept {
    for (const auto& lane : lanes_) {
        if (lane.id() == id) {
            return &lane;
        }
    }
    return nullptr;
}

} // namespace saudade::model
