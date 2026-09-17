#pragma once

#include <saudade/model/pattern.hpp>
#include <saudade/model/pattern_compiler.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/time/time_types.hpp>

#include <QObject>
#include <QTimer>

#include <memory>
#include <cstdint>

namespace saudade::ui {

/// Application controller interfacing the Qt GUI shell with Saudade's core
/// musical Pattern model and realtime AudioEngine.
class EditorController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(double currentBeat READ currentBeat NOTIFY currentBeatChanged)
    Q_PROPERTY(double bpm READ bpm CONSTANT)
    Q_PROPERTY(double patternLength READ patternLength CONSTANT)

public:
    explicit EditorController(audio::AudioEngine& engine,
                              double sample_rate = 48000.0,
                              QObject* parent = nullptr);
    ~EditorController() override = default;

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] double currentBeat() const noexcept { return current_beat_; }
    [[nodiscard]] double bpm() const noexcept;
    [[nodiscard]] double patternLength() const noexcept;

    [[nodiscard]] const model::Pattern& pattern() const noexcept { return pattern_; }
    [[nodiscard]] const model::NoteSequence* active_sequence() const noexcept;
    [[nodiscard]] model::NoteSequence* active_sequence() noexcept;

    // --- Note Editing API (Disabled during playback) ---

    Q_INVOKABLE uint64_t addNote(double start_beat, double duration_beats, double pitch, float velocity = 0.8f);
    Q_INVOKABLE bool moveNote(uint64_t note_id, double new_start_beat, double new_pitch);
    Q_INVOKABLE bool resizeNote(uint64_t note_id, double new_duration_beats);
    Q_INVOKABLE bool removeNote(uint64_t note_id);

    // --- Transport Playback API ---

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();

    void set_sample_rate(double sr) noexcept { sample_rate_ = sr; }

signals:
    void isPlayingChanged(bool isPlaying);
    void currentBeatChanged(double currentBeat);
    void notesChanged();

private slots:
    void onTimerTick();

private:
    audio::AudioEngine& engine_;
    double sample_rate_{48000.0};
    model::Pattern pattern_;
    model::LaneId active_lane_id_{0};

    QTimer playhead_timer_;
    double current_beat_{0.0};
    bool is_playing_{false};
};

} // namespace saudade::ui
