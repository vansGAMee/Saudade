#include <saudade/ui/editor_controller.hpp>

#include <cmath>

namespace saudade::ui {

EditorController::EditorController(audio::AudioEngine& engine,
                                   double sample_rate,
                                   QObject* parent)
    : QObject(parent)
    , engine_(engine)
    , sample_rate_(sample_rate > 0.0 ? sample_rate : 48000.0)
    , pattern_(1, "ComposerPattern", time::BeatDuration::from_beats(4)) {

    active_lane_id_ = pattern_.add_lane("SynthLead");

    connect(&playhead_timer_, &QTimer::timeout, this, &EditorController::onTimerTick);
    playhead_timer_.start(16); // ~60 Hz
}

bool EditorController::isPlaying() const noexcept {
    return is_playing_;
}

double EditorController::bpm() const noexcept {
    return engine_.transport().tempo_map().bpm();
}

double EditorController::patternLength() const noexcept {
    return pattern_.length().to_double();
}

const model::NoteSequence* EditorController::active_sequence() const noexcept {
    const auto* lane = pattern_.find_lane(active_lane_id_);
    return lane ? &lane->notes() : nullptr;
}

model::NoteSequence* EditorController::active_sequence() noexcept {
    auto* lane = pattern_.find_lane(active_lane_id_);
    return lane ? &lane->notes() : nullptr;
}

uint64_t EditorController::addNote(double start_beat, double duration_beats, double pitch, float velocity) {
    if (isPlaying()) {
        return 0;
    }
    auto* seq = active_sequence();
    if (!seq) {
        return 0;
    }

    const auto start_ticks = static_cast<int64_t>(std::llround(start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    const auto dur_ticks = static_cast<int64_t>(std::llround(duration_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));

    if (start_ticks < 0 || dur_ticks <= 0) {
        return 0;
    }

    const auto id_opt = seq->add_note(
        time::BeatPosition::from_ticks(start_ticks),
        time::BeatDuration::from_ticks(dur_ticks),
        pitch,
        velocity
    );

    if (id_opt.has_value()) {
        emit notesChanged();
        return *id_opt;
    }
    return 0;
}

bool EditorController::moveNote(uint64_t note_id, double new_start_beat, double new_pitch) {
    if (isPlaying()) {
        return false;
    }
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }

    const auto start_ticks = static_cast<int64_t>(std::llround(new_start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    if (start_ticks < 0) {
        return false;
    }

    const bool moved = seq->move_note(note_id, time::BeatPosition::from_ticks(start_ticks));
    const bool pitched = seq->update_pitch(note_id, new_pitch);

    if (moved || pitched) {
        emit notesChanged();
        return true;
    }
    return false;
}

bool EditorController::resizeNote(uint64_t note_id, double new_duration_beats) {
    if (isPlaying()) {
        return false;
    }
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }

    const auto dur_ticks = static_cast<int64_t>(std::llround(new_duration_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    if (dur_ticks <= 0) {
        return false;
    }

    if (seq->resize_note(note_id, time::BeatDuration::from_ticks(dur_ticks))) {
        emit notesChanged();
        return true;
    }
    return false;
}

bool EditorController::removeNote(uint64_t note_id) {
    if (isPlaying()) {
        return false;
    }
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }

    if (seq->remove_note(note_id)) {
        emit notesChanged();
        return true;
    }
    return false;
}

void EditorController::play() {
    if (isPlaying()) {
        return;
    }

    // 1. Transport must be Stopped
    engine_.stop();

    // 2. Safely prepare event ingress: flush any pending queue events
    engine_.flush_events();

    // 3. Compile current Pattern control-side
    const auto compiled_events = model::PatternCompiler::compile(
        pattern_,
        engine_.transport().tempo_map(),
        sample_rate_
    );

    // 4. Ingress compiled events to AudioEngine
    for (const auto& ev : compiled_events) {
        engine_.schedule_event(ev);
    }

    // 5. Seek transport to sample 0
    engine_.seek_samples(0);

    // 6. Start playback
    engine_.play();

    is_playing_ = true;
    emit isPlayingChanged(true);
}

void EditorController::stop() {
    engine_.stop();
    is_playing_ = false;
    current_beat_ = 0.0;
    emit isPlayingChanged(false);
    emit currentBeatChanged(0.0);
}

void EditorController::onTimerTick() {
    const bool engine_playing = engine_.transport().is_playing();
    if (engine_playing != is_playing_) {
        is_playing_ = engine_playing;
        emit isPlayingChanged(is_playing_);
    }

    if (is_playing_) {
        const auto cur_sample = engine_.transport().current_sample();
        const auto cur_beat = engine_.transport().tempo_map().sample_to_beat(cur_sample, sample_rate_);
        current_beat_ = cur_beat.to_double();
        emit currentBeatChanged(current_beat_);
    }
}

} // namespace saudade::ui
