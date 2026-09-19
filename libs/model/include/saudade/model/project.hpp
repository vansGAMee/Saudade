#pragma once

#include <saudade/model/pattern.hpp>
#include <saudade/time/time_types.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace saudade::model {

using TrackId = uint64_t;
using ClipId = uint64_t;

struct MixerState {
    float gain_db{0.0f};
    float pan{0.0f};
    bool muted{false};
    bool solo{false};
};

struct SynthPatch {
    float attack_seconds{0.008f};
    float decay_seconds{0.20f};
    float sustain{0.65f};
    float release_seconds{0.12f};
    float cutoff_hz{3200.0f};
    float resonance{0.18f};
    float character{0.78f};
};

struct ClipInstance {
    ClipId id{0};
    PatternId pattern_id{0};
    TrackId track_id{0};
    time::BeatPosition start{};
    time::BeatDuration duration{time::BeatDuration::from_beats(4)};
    time::BeatPosition pattern_offset{};

    [[nodiscard]] bool is_valid() const noexcept {
        return id != 0 && pattern_id != 0 && track_id != 0 &&
               start.ticks >= 0 && duration.ticks > 0 &&
               pattern_offset.ticks >= 0;
    }
};

struct Track {
    TrackId id{0};
    std::string name{"Instrument"};
    MixerState mixer{};
};

/// Canonical editable song model. UI coordinates, selection, and other editor
/// state deliberately do not live here.
class Project {
public:
    Project();

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }
    [[nodiscard]] double bpm() const noexcept { return bpm_; }
    bool set_bpm(double bpm) noexcept;
    [[nodiscard]] float master_gain_db() const noexcept { return master_gain_db_; }
    void set_master_gain_db(float gain_db) noexcept;
    [[nodiscard]] const SynthPatch& synth_patch() const noexcept { return synth_patch_; }
    void set_synth_patch(SynthPatch patch) noexcept;

    TrackId add_track(std::string name = "Instrument");
    bool add_track(Track track);
    bool remove_track(TrackId id) noexcept;
    [[nodiscard]] Track* find_track(TrackId id) noexcept;
    [[nodiscard]] const Track* find_track(TrackId id) const noexcept;

    PatternId add_pattern(std::string name = "Pattern",
                          time::BeatDuration length = time::BeatDuration::from_beats(16));
    bool add_pattern(Pattern pattern);
    bool remove_pattern(PatternId id) noexcept;
    [[nodiscard]] Pattern* find_pattern(PatternId id) noexcept;
    [[nodiscard]] const Pattern* find_pattern(PatternId id) const noexcept;

    ClipId add_clip(TrackId track_id, PatternId pattern_id,
                    time::BeatPosition start,
                    time::BeatDuration duration);
    bool add_clip(ClipInstance clip);
    bool remove_clip(ClipId id) noexcept;
    [[nodiscard]] ClipInstance* find_clip(ClipId id) noexcept;
    [[nodiscard]] const ClipInstance* find_clip(ClipId id) const noexcept;

    [[nodiscard]] const std::vector<Track>& tracks() const noexcept { return tracks_; }
    [[nodiscard]] const std::vector<Pattern>& patterns() const noexcept { return patterns_; }
    [[nodiscard]] const std::vector<ClipInstance>& clips() const noexcept { return clips_; }

    [[nodiscard]] time::BeatPosition end_beat() const noexcept;
    void clear();

private:
    std::string name_{"Untitled"};
    double bpm_{120.0};
    float master_gain_db_{0.0f};
    SynthPatch synth_patch_{};
    std::vector<Track> tracks_;
    std::vector<Pattern> patterns_;
    std::vector<ClipInstance> clips_;
    TrackId next_track_id_{1};
    PatternId next_pattern_id_{1};
    ClipId next_clip_id_{1};
};

} // namespace saudade::model
