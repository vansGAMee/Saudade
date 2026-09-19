#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <atomic>
#include <memory>
#include <unordered_set>
#include <vector>
#include <functional>

#include <saudade/model/pattern.hpp>
#include <saudade/model/pattern_compiler.hpp>
#include <saudade/model/project.hpp>
#include <saudade/audio/engine.hpp>
#include <saudade/time/time_types.hpp>
#include <saudade/ui/editor_commands.hpp>

namespace saudade::ui {

/// Application controller interfacing the Qt GUI shell with Saudade's core
/// musical Pattern model, CommandHistory undo/redo, and realtime AudioEngine.
class EditorController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(double currentBeat READ currentBeat NOTIFY currentBeatChanged)
    Q_PROPERTY(double bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    Q_PROPERTY(double patternLength READ patternLength CONSTANT)
    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY loopEnabledChanged)
    Q_PROPERTY(double loopStartBeat READ loopStartBeat WRITE setLoopStartBeat NOTIFY loopRangeChanged)
    Q_PROPERTY(double loopEndBeat READ loopEndBeat WRITE setLoopEndBeat NOTIFY loopRangeChanged)
    Q_PROPERTY(bool metronomeEnabled READ metronomeEnabled WRITE setMetronomeEnabled NOTIFY metronomeEnabledChanged)
    Q_PROPERTY(float metronomeVolume READ metronomeVolume WRITE setMetronomeVolume NOTIFY metronomeVolumeChanged)
    Q_PROPERTY(QString snapStep READ snapStep WRITE setSnapStep NOTIFY snapStepChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(bool projectDirty READ projectDirty NOTIFY projectDirtyChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectPathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList tracksData READ tracksData NOTIFY arrangementChanged)
    Q_PROPERTY(QVariantList clipsData READ clipsData NOTIFY arrangementChanged)
    Q_PROPERTY(bool exportInProgress READ exportInProgress NOTIFY exportStateChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(float masterGainDb READ masterGainDb WRITE setMasterGainDb NOTIFY masterGainChanged)
    Q_PROPERTY(float meterLeft READ meterLeft NOTIFY metersChanged)
    Q_PROPERTY(float meterRight READ meterRight NOTIFY metersChanged)
    Q_PROPERTY(float synthAttack READ synthAttack WRITE setSynthAttack NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthDecay READ synthDecay WRITE setSynthDecay NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthSustain READ synthSustain WRITE setSynthSustain NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthRelease READ synthRelease WRITE setSynthRelease NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthCutoff READ synthCutoff WRITE setSynthCutoff NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthResonance READ synthResonance WRITE setSynthResonance NOTIFY synthPatchChanged)
    Q_PROPERTY(float synthCharacter READ synthCharacter WRITE setSynthCharacter NOTIFY synthPatchChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryAvailableChanged)

public:
    explicit EditorController(audio::AudioEngine& engine,
                              double sample_rate = 48000.0,
                              QObject* parent = nullptr);
    ~EditorController() override = default;

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] double currentBeat() const noexcept { return current_beat_; }
    [[nodiscard]] double bpm() const noexcept;
    [[nodiscard]] double patternLength() const noexcept;

    [[nodiscard]] bool loopEnabled() const noexcept;
    [[nodiscard]] double loopStartBeat() const noexcept { return loop_start_beat_; }
    [[nodiscard]] double loopEndBeat() const noexcept { return loop_end_beat_; }

    [[nodiscard]] bool metronomeEnabled() const noexcept;
    [[nodiscard]] float metronomeVolume() const noexcept;
    [[nodiscard]] const QString& snapStep() const noexcept { return snap_step_; }
    [[nodiscard]] time::BeatDuration snapDuration() const noexcept;

    [[nodiscard]] bool canUndo() const noexcept { return history_.can_undo(); }
    [[nodiscard]] bool canRedo() const noexcept { return history_.can_redo(); }
    [[nodiscard]] int selectionCount() const noexcept { return static_cast<int>(selected_note_ids_.size()); }
    [[nodiscard]] bool projectDirty() const noexcept { return project_dirty_; }
    [[nodiscard]] const QString& projectPath() const noexcept { return project_path_; }
    [[nodiscard]] const QString& lastError() const noexcept { return last_error_; }
    [[nodiscard]] QVariantList tracksData() const;
    [[nodiscard]] QVariantList clipsData() const;
    [[nodiscard]] bool exportInProgress() const noexcept { return export_in_progress_; }
    [[nodiscard]] double exportProgress() const noexcept { return export_progress_; }
    [[nodiscard]] float masterGainDb() const noexcept;
    [[nodiscard]] float meterLeft() const noexcept { return meter_left_; }
    [[nodiscard]] float meterRight() const noexcept { return meter_right_; }
    [[nodiscard]] float synthAttack() const noexcept { return project_.synth_patch().attack_seconds; }
    [[nodiscard]] float synthDecay() const noexcept { return project_.synth_patch().decay_seconds; }
    [[nodiscard]] float synthSustain() const noexcept { return project_.synth_patch().sustain; }
    [[nodiscard]] float synthRelease() const noexcept { return project_.synth_patch().release_seconds; }
    [[nodiscard]] float synthCutoff() const noexcept { return project_.synth_patch().cutoff_hz; }
    [[nodiscard]] float synthResonance() const noexcept { return project_.synth_patch().resonance; }
    [[nodiscard]] float synthCharacter() const noexcept { return project_.synth_patch().character; }
    [[nodiscard]] bool recoveryAvailable() const noexcept { return recovery_available_; }

    [[nodiscard]] const model::Project& project() const noexcept { return project_; }
    [[nodiscard]] const model::Pattern& pattern() const noexcept;
    [[nodiscard]] const model::NoteSequence* active_sequence() const noexcept;
    [[nodiscard]] model::NoteSequence* active_sequence() noexcept;

    // --- Note Editing API ---

    Q_INVOKABLE uint64_t addNote(double start_beat, double duration_beats, double pitch, float velocity = 0.8f);
    uint64_t beginNoteCreation(double start_beat, double duration_beats,
                               double pitch, float velocity = 0.8f);
    void commitNoteCreation(uint64_t note_id);
    void commitTransformGesture(std::vector<NoteDelta> moves,
                                std::vector<ResizeDelta> resizes);
    Q_INVOKABLE bool moveNote(uint64_t note_id, double new_start_beat, double new_pitch);
    Q_INVOKABLE bool resizeNote(uint64_t note_id, double new_duration_beats);
    Q_INVOKABLE bool removeNote(uint64_t note_id);
    Q_INVOKABLE bool setNoteVelocity(uint64_t note_id, float velocity);

    // --- Selection & Multi-Note Editing ---

    Q_INVOKABLE void selectNote(uint64_t note_id, bool add_to_selection = false);
    Q_INVOKABLE void deselectNote(uint64_t note_id);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE bool isNoteSelected(uint64_t note_id) const noexcept;
    [[nodiscard]] const std::unordered_set<events::NoteId>& selectedNoteIds() const noexcept { return selected_note_ids_; }
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void setSelectedVelocity(float velocity);
    Q_INVOKABLE void moveSelected(double delta_beats, int delta_pitch);
    Q_INVOKABLE void resizeSelected(double delta_beats);
    Q_INVOKABLE void nudgePitchSelected(int semitones);
    Q_INVOKABLE void nudgeBeatSelected(int grid_steps);
    Q_INVOKABLE void beginVelocityGesture(uint64_t note_id);
    Q_INVOKABLE void previewVelocityGesture(float velocity);
    Q_INVOKABLE void commitVelocityGesture();

    // --- Clipboard & Duplication ---

    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste(double target_beat = -1.0);
    Q_INVOKABLE void duplicate();

    // --- Semantic Undo / Redo ---

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void beginBatch();
    Q_INVOKABLE void commitBatch();
    Q_INVOKABLE void discardBatch();

    // --- Transport, Tempo, Loop & Metronome ---

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekBeats(double beat);

    Q_INVOKABLE void setBpm(double new_bpm);
    Q_INVOKABLE void setLoopEnabled(bool enabled);
    Q_INVOKABLE void setLoopRange(double start_beat, double end_beat);
    Q_INVOKABLE void setLoopStartBeat(double start_beat);
    Q_INVOKABLE void setLoopEndBeat(double end_beat);
    Q_INVOKABLE void setMetronomeEnabled(bool enabled);
    Q_INVOKABLE void setMetronomeVolume(float vol);
    Q_INVOKABLE void setSnapStep(const QString& step);

    // --- Audition / Preview API ---

    Q_INVOKABLE void auditionNoteOn(double pitch, float velocity = 0.8f);
    Q_INVOKABLE void auditionNoteOff();

    // --- UI Data Helpers ---

    Q_INVOKABLE QVariantList getNotesData() const;
    Q_INVOKABLE QVariantMap getVelocityStats() const;
    Q_INVOKABLE void quantizeSelected();
    Q_INVOKABLE void humanizeSelected();
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openProject(const QString& path);
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE bool saveProjectAs(const QString& path);
    Q_INVOKABLE bool importMidi(const QString& path, double start_beat = 0.0);
    Q_INVOKABLE bool exportWav(const QString& path);
    Q_INVOKABLE void cancelExport();

    // --- Arrangement / Mixer ---
    Q_INVOKABLE uint64_t addTrack(const QString& name = QStringLiteral("Instrument"));
    Q_INVOKABLE bool renameTrack(uint64_t track_id, const QString& name);
    Q_INVOKABLE bool deleteTrack(uint64_t track_id);
    Q_INVOKABLE void selectTrack(uint64_t track_id);
    Q_INVOKABLE uint64_t createPattern(uint64_t track_id, double start_beat,
                                       double length_beats = 16.0);
    Q_INVOKABLE bool openClip(uint64_t clip_id);
    Q_INVOKABLE bool moveClip(uint64_t clip_id, double start_beat,
                              uint64_t track_id);
    Q_INVOKABLE bool resizeClip(uint64_t clip_id, double duration_beats);
    Q_INVOKABLE uint64_t duplicateClip(uint64_t clip_id);
    Q_INVOKABLE bool deleteClip(uint64_t clip_id);
    Q_INVOKABLE void selectClip(uint64_t clip_id);
    Q_INVOKABLE bool setTrackGain(uint64_t track_id, float gain_db);
    Q_INVOKABLE bool setTrackPan(uint64_t track_id, float pan);
    Q_INVOKABLE bool setTrackMute(uint64_t track_id, bool muted);
    Q_INVOKABLE bool setTrackSolo(uint64_t track_id, bool solo);
    Q_INVOKABLE void setMasterGainDb(float gain_db);
    Q_INVOKABLE void setSynthAttack(float value);
    Q_INVOKABLE void setSynthDecay(float value);
    Q_INVOKABLE void setSynthSustain(float value);
    Q_INVOKABLE void setSynthRelease(float value);
    Q_INVOKABLE void setSynthCutoff(float value);
    Q_INVOKABLE void setSynthResonance(float value);
    Q_INVOKABLE void setSynthCharacter(float value);
    Q_INVOKABLE bool recoverAutosave();
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void arrangementUndo();
    Q_INVOKABLE void arrangementRedo();

    void set_sample_rate(double sr) noexcept { sample_rate_ = sr; }
    void syncPatternToEngine(bool mark_dirty = true);

signals:
    void isPlayingChanged(bool isPlaying);
    void currentBeatChanged(double currentBeat);
    void bpmChanged(double bpm);
    void loopEnabledChanged(bool loopEnabled);
    void loopRangeChanged();
    void metronomeEnabledChanged(bool enabled);
    void metronomeVolumeChanged(float vol);
    void snapStepChanged(const QString& step);
    void undoRedoChanged();
    void selectionChanged();
    void notesChanged();
    void projectDirtyChanged(bool dirty);
    void projectPathChanged(const QString& path);
    void lastErrorChanged(const QString& error);
    void arrangementChanged();
    void exportStateChanged(bool active);
    void exportProgressChanged(double progress);
    void masterGainChanged(float gain_db);
    void metersChanged();
    void synthPatchChanged();
    void recoveryAvailableChanged(bool available);

private slots:
    void onTimerTick();

private:
    [[nodiscard]] model::Pattern* active_pattern() noexcept;
    [[nodiscard]] const model::Pattern* active_pattern() const noexcept;
    void set_project_dirty(bool dirty);
    void set_last_error(QString error);
    void reset_project_model();
    [[nodiscard]] std::vector<events::TimelineEvent> compile_project_events() const;
    void rebuild_synth_plan();
    void commit_synth_patch(model::SynthPatch patch);
    void autosave_if_needed();
    void remove_recovery_file();
    void push_arrangement_edit(std::function<void()> undo,
                               std::function<void()> redo);
    void arrangement_edit_applied();

    struct ArrangementEdit {
        std::function<void()> undo;
        std::function<void()> redo;
    };

    audio::AudioEngine& engine_;
    double sample_rate_{48000.0};
    model::Project project_;
    model::TrackId active_track_id_{0};
    model::PatternId active_pattern_id_{0};
    model::LaneId active_lane_id_{0};
    model::ClipId selected_clip_id_{0};

    CommandHistory history_;
    std::unordered_set<events::NoteId> selected_note_ids_;
    std::vector<model::Note> clipboard_;
    std::vector<VelocityDelta> velocity_gesture_;
    std::vector<ArrangementEdit> arrangement_undo_;
    std::vector<ArrangementEdit> arrangement_redo_;

    QTimer playhead_timer_;
    QTimer autosave_timer_;
    double current_beat_{0.0};
    bool is_playing_{false};

    double loop_start_beat_{0.0};
    double loop_end_beat_{16.0}; // default 4-bar loop (16 beats)
    QString snap_step_{"1/16"};
    QString project_path_;
    QString last_error_;
    bool project_dirty_{false};
    bool export_in_progress_{false};
    double export_progress_{0.0};
    std::atomic<bool> export_cancel_requested_{false};
    float meter_left_{0.0f};
    float meter_right_{0.0f};
    QString recovery_path_;
    bool recovery_available_{false};
};

} // namespace saudade::ui
