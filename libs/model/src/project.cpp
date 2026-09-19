#include <saudade/model/project.hpp>

#include <algorithm>
#include <cmath>

namespace saudade::model {

Project::Project() = default;

bool Project::set_bpm(double bpm) noexcept {
    if (!std::isfinite(bpm) || bpm < 20.0 || bpm > 400.0) {
        return false;
    }
    bpm_ = bpm;
    return true;
}

void Project::set_master_gain_db(float gain_db) noexcept {
    master_gain_db_ = std::clamp(gain_db, -60.0f, 12.0f);
}

void Project::set_synth_patch(SynthPatch patch) noexcept {
    patch.attack_seconds = std::clamp(patch.attack_seconds, 0.001f, 2.0f);
    patch.decay_seconds = std::clamp(patch.decay_seconds, 0.005f, 4.0f);
    patch.sustain = std::clamp(patch.sustain, 0.0f, 1.0f);
    patch.release_seconds = std::clamp(patch.release_seconds, 0.005f, 6.0f);
    patch.cutoff_hz = std::clamp(patch.cutoff_hz, 40.0f, 18000.0f);
    patch.resonance = std::clamp(patch.resonance, 0.0f, 0.95f);
    patch.character = std::clamp(patch.character, 0.0f, 1.0f);
    synth_patch_ = patch;
}

TrackId Project::add_track(std::string name) {
    const auto id = next_track_id_++;
    tracks_.push_back(Track{.id = id, .name = std::move(name)});
    return id;
}

bool Project::add_track(Track track) {
    if (track.id == 0 || find_track(track.id) != nullptr) {
        return false;
    }
    next_track_id_ = std::max(next_track_id_, track.id + 1);
    tracks_.push_back(std::move(track));
    return true;
}

bool Project::remove_track(TrackId id) noexcept {
    const auto before = tracks_.size();
    std::erase_if(tracks_, [id](const Track& track) { return track.id == id; });
    if (tracks_.size() == before) {
        return false;
    }
    std::erase_if(clips_, [id](const ClipInstance& clip) { return clip.track_id == id; });
    return true;
}

Track* Project::find_track(TrackId id) noexcept {
    const auto it = std::find_if(tracks_.begin(), tracks_.end(),
                                 [id](const Track& track) { return track.id == id; });
    return it == tracks_.end() ? nullptr : &*it;
}

const Track* Project::find_track(TrackId id) const noexcept {
    const auto it = std::find_if(tracks_.begin(), tracks_.end(),
                                 [id](const Track& track) { return track.id == id; });
    return it == tracks_.end() ? nullptr : &*it;
}

PatternId Project::add_pattern(std::string name, time::BeatDuration length) {
    const auto id = next_pattern_id_++;
    patterns_.emplace_back(id, std::move(name), length);
    return id;
}

bool Project::add_pattern(Pattern pattern) {
    if (pattern.id() == 0 || find_pattern(pattern.id()) != nullptr) {
        return false;
    }
    next_pattern_id_ = std::max(next_pattern_id_, pattern.id() + 1);
    patterns_.push_back(std::move(pattern));
    return true;
}

bool Project::remove_pattern(PatternId id) noexcept {
    const auto before = patterns_.size();
    std::erase_if(patterns_, [id](const Pattern& pattern) { return pattern.id() == id; });
    if (patterns_.size() == before) {
        return false;
    }
    std::erase_if(clips_, [id](const ClipInstance& clip) { return clip.pattern_id == id; });
    return true;
}

Pattern* Project::find_pattern(PatternId id) noexcept {
    const auto it = std::find_if(patterns_.begin(), patterns_.end(),
                                 [id](const Pattern& pattern) { return pattern.id() == id; });
    return it == patterns_.end() ? nullptr : &*it;
}

const Pattern* Project::find_pattern(PatternId id) const noexcept {
    const auto it = std::find_if(patterns_.begin(), patterns_.end(),
                                 [id](const Pattern& pattern) { return pattern.id() == id; });
    return it == patterns_.end() ? nullptr : &*it;
}

ClipId Project::add_clip(TrackId track_id, PatternId pattern_id,
                         time::BeatPosition start, time::BeatDuration duration) {
    ClipInstance clip{
        .id = next_clip_id_++,
        .pattern_id = pattern_id,
        .track_id = track_id,
        .start = start,
        .duration = duration,
    };
    if (!clip.is_valid() || find_track(track_id) == nullptr || find_pattern(pattern_id) == nullptr) {
        return 0;
    }
    clips_.push_back(clip);
    return clip.id;
}

bool Project::add_clip(ClipInstance clip) {
    if (!clip.is_valid() || find_clip(clip.id) != nullptr ||
        find_track(clip.track_id) == nullptr || find_pattern(clip.pattern_id) == nullptr) {
        return false;
    }
    next_clip_id_ = std::max(next_clip_id_, clip.id + 1);
    clips_.push_back(clip);
    return true;
}

bool Project::remove_clip(ClipId id) noexcept {
    const auto before = clips_.size();
    std::erase_if(clips_, [id](const ClipInstance& clip) { return clip.id == id; });
    return clips_.size() != before;
}

ClipInstance* Project::find_clip(ClipId id) noexcept {
    const auto it = std::find_if(clips_.begin(), clips_.end(),
                                 [id](const ClipInstance& clip) { return clip.id == id; });
    return it == clips_.end() ? nullptr : &*it;
}

const ClipInstance* Project::find_clip(ClipId id) const noexcept {
    const auto it = std::find_if(clips_.begin(), clips_.end(),
                                 [id](const ClipInstance& clip) { return clip.id == id; });
    return it == clips_.end() ? nullptr : &*it;
}

time::BeatPosition Project::end_beat() const noexcept {
    int64_t end_ticks = 0;
    for (const auto& clip : clips_) {
        end_ticks = std::max(end_ticks, clip.start.ticks + clip.duration.ticks);
    }
    return time::BeatPosition::from_ticks(end_ticks);
}

void Project::clear() {
    name_ = "Untitled";
    bpm_ = 120.0;
    master_gain_db_ = 0.0f;
    synth_patch_ = {};
    tracks_.clear();
    patterns_.clear();
    clips_.clear();
    next_track_id_ = 1;
    next_pattern_id_ = 1;
    next_clip_id_ = 1;
}

} // namespace saudade::model
