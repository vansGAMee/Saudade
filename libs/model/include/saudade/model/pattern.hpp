#pragma once

#include <saudade/model/note_sequence.hpp>
#include <saudade/time/time_types.hpp>

#include <string>
#include <vector>
#include <cstdint>

namespace saudade::model {

using LaneId = uint64_t;
using PatternId = uint64_t;

/// A single lane within a pattern containing an independent NoteSequence.
class PatternLane {
public:
    explicit PatternLane(LaneId id, std::string name = "") noexcept
        : id_(id), name_(std::move(name)) {}

    [[nodiscard]] LaneId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const NoteSequence& notes() const noexcept { return notes_; }
    [[nodiscard]] NoteSequence& notes() noexcept { return notes_; }

private:
    LaneId id_{0};
    std::string name_;
    NoteSequence notes_;
};

/// Multi-lane musical pattern container.
/// Holds a length in BeatDuration and one or more independent PatternLanes.
class Pattern {
public:
    explicit Pattern(PatternId id = 1,
                     std::string name = "Pattern",
                     time::BeatDuration length = time::BeatDuration::from_beats(4)) noexcept;

    [[nodiscard]] PatternId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] time::BeatDuration length() const noexcept { return length_; }
    bool set_length(time::BeatDuration length) noexcept;

    /// Adds a new lane with a stable unique LaneId.
    LaneId add_lane(std::string name = "");

    /// Removes a lane by its stable LaneId.
    bool remove_lane(LaneId id) noexcept;

    [[nodiscard]] PatternLane* find_lane(LaneId id) noexcept;
    [[nodiscard]] const PatternLane* find_lane(LaneId id) const noexcept;

    [[nodiscard]] const std::vector<PatternLane>& lanes() const noexcept { return lanes_; }
    [[nodiscard]] std::vector<PatternLane>& lanes() noexcept { return lanes_; }
    [[nodiscard]] size_t num_lanes() const noexcept { return lanes_.size(); }

private:
    PatternId id_{1};
    std::string name_;
    time::BeatDuration length_{time::BeatDuration::from_beats(4)};
    std::vector<PatternLane> lanes_;
    LaneId next_lane_id_{1};
};

} // namespace saudade::model
