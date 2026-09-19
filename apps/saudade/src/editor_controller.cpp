#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/coordinates.hpp>
#include <saudade/ui/project_file.hpp>
#include <saudade/ui/project_exporter.hpp>
#include <saudade/model/midi_import.hpp>
#include <saudade/graph/graph_compiler.hpp>
#include <saudade/graph/graph_model.hpp>

#include <cmath>
#include <algorithm>
#include <numeric>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

namespace saudade::ui {

EditorController::EditorController(audio::AudioEngine& engine,
                                   double sample_rate,
                                   QObject* parent)
    : QObject(parent)
    , engine_(engine)
    , sample_rate_(sample_rate > 0.0 ? sample_rate : 48000.0) {
    const QString data_dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data_dir);
    recovery_path_ = QDir(data_dir).filePath("recovery.dawproj");
    recovery_available_ = QFileInfo::exists(recovery_path_);
    reset_project_model();

    // Initialize 4-bar loop (16 beats)
    setLoopRange(0.0, 16.0);
    setLoopEnabled(true);

    connect(&playhead_timer_, &QTimer::timeout, this, &EditorController::onTimerTick);
    playhead_timer_.start(16); // ~60 Hz
    connect(&autosave_timer_, &QTimer::timeout,
            this, &EditorController::autosave_if_needed);
    autosave_timer_.start(20000);
}

bool EditorController::isPlaying() const noexcept {
    return is_playing_;
}

double EditorController::bpm() const noexcept {
    return engine_.transport().tempo_map().bpm();
}

double EditorController::patternLength() const noexcept {
    const auto* current = active_pattern();
    return current ? current->length().to_double() : 0.0;
}

const model::Pattern& EditorController::pattern() const noexcept {
    return *active_pattern();
}

model::Pattern* EditorController::active_pattern() noexcept {
    return project_.find_pattern(active_pattern_id_);
}

const model::Pattern* EditorController::active_pattern() const noexcept {
    return project_.find_pattern(active_pattern_id_);
}

void EditorController::set_project_dirty(bool dirty) {
    if (project_dirty_ == dirty) {
        return;
    }
    project_dirty_ = dirty;
    emit projectDirtyChanged(dirty);
}

void EditorController::autosave_if_needed() {
    if (!project_dirty_ || export_in_progress_) {
        return;
    }
    QString error;
    if (ProjectFile::save(project_, recovery_path_, &error)) {
        if (!recovery_available_) {
            recovery_available_ = true;
            emit recoveryAvailableChanged(true);
        }
    } else {
        set_last_error(QStringLiteral("Autosave failed: %1").arg(error));
    }
}

void EditorController::remove_recovery_file() {
    if (QFileInfo::exists(recovery_path_)) {
        QFile::remove(recovery_path_);
    }
    if (recovery_available_) {
        recovery_available_ = false;
        emit recoveryAvailableChanged(false);
    }
}

bool EditorController::recoverAutosave() {
    if (!recovery_available_ || !openProject(recovery_path_)) {
        return false;
    }
    project_path_.clear();
    set_project_dirty(true);
    emit projectPathChanged(project_path_);
    return true;
}

void EditorController::discardRecovery() {
    remove_recovery_file();
}

void EditorController::push_arrangement_edit(std::function<void()> undo,
                                             std::function<void()> redo) {
    arrangement_undo_.push_back(
        ArrangementEdit{std::move(undo), std::move(redo)});
    arrangement_redo_.clear();
}

void EditorController::arrangement_edit_applied() {
    selected_clip_id_ = 0;
    syncPatternToEngine();
    emit arrangementChanged();
}

void EditorController::arrangementUndo() {
    if (arrangement_undo_.empty()) {
        return;
    }
    auto edit = std::move(arrangement_undo_.back());
    arrangement_undo_.pop_back();
    edit.undo();
    arrangement_redo_.push_back(std::move(edit));
    arrangement_edit_applied();
}

void EditorController::arrangementRedo() {
    if (arrangement_redo_.empty()) {
        return;
    }
    auto edit = std::move(arrangement_redo_.back());
    arrangement_redo_.pop_back();
    edit.redo();
    arrangement_undo_.push_back(std::move(edit));
    arrangement_edit_applied();
}

void EditorController::set_last_error(QString error) {
    if (last_error_ == error) {
        return;
    }
    last_error_ = std::move(error);
    emit lastErrorChanged(last_error_);
}

void EditorController::reset_project_model() {
    project_.clear();
    active_track_id_ = project_.add_track("Instrument 1");
    active_pattern_id_ = project_.add_pattern(
        "Pattern 1", time::BeatDuration::from_beats(16));
    auto* initial_pattern = project_.find_pattern(active_pattern_id_);
    active_lane_id_ = initial_pattern->add_lane("Synth");
    project_.add_clip(active_track_id_, active_pattern_id_,
                      time::BeatPosition::zero(),
                      time::BeatDuration::from_beats(16));
    engine_.set_master_gain_db(project_.master_gain_db());
    rebuild_synth_plan();
}

bool EditorController::loopEnabled() const noexcept {
    return engine_.transport().is_loop_enabled();
}

bool EditorController::metronomeEnabled() const noexcept {
    return engine_.is_metronome_enabled();
}

float EditorController::metronomeVolume() const noexcept {
    return engine_.metronome_volume();
}

float EditorController::masterGainDb() const noexcept {
    return engine_.master_gain_db();
}

time::BeatDuration EditorController::snapDuration() const noexcept {
    if (snap_step_ == "1 Bar") {
        return time::BeatDuration::from_beats(4);
    } else if (snap_step_ == "1/2") {
        return time::BeatDuration::from_beats(2);
    } else if (snap_step_ == "1/4") {
        return time::BeatDuration::from_beats(1);
    } else if (snap_step_ == "1/8") {
        return time::BeatDuration::from_fraction(1, 2);
    } else if (snap_step_ == "1/16") {
        return time::BeatDuration::from_fraction(1, 4);
    } else if (snap_step_ == "1/32") {
        return time::BeatDuration::from_fraction(1, 8);
    }
    // "Off" or default
    return time::BeatDuration::from_ticks(1);
}

const model::NoteSequence* EditorController::active_sequence() const noexcept {
    const auto* current = active_pattern();
    const auto* lane = current ? current->find_lane(active_lane_id_) : nullptr;
    return lane ? &lane->notes() : nullptr;
}

model::NoteSequence* EditorController::active_sequence() noexcept {
    auto* current = active_pattern();
    auto* lane = current ? current->find_lane(active_lane_id_) : nullptr;
    return lane ? &lane->notes() : nullptr;
}

uint64_t EditorController::addNote(double start_beat, double duration_beats, double pitch, float velocity) {
    const auto id = beginNoteCreation(start_beat, duration_beats, pitch, velocity);
    if (id != 0) {
        commitNoteCreation(id);
    }
    return id;
}

uint64_t EditorController::beginNoteCreation(double start_beat,
                                             double duration_beats,
                                             double pitch,
                                             float velocity) {
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
        std::clamp(velocity, 0.0f, 1.0f)
    );

    if (id_opt.has_value()) {
        syncPatternToEngine();
        emit notesChanged();
        return *id_opt;
    }
    return 0;
}

void EditorController::commitNoteCreation(uint64_t note_id) {
    const auto* seq = active_sequence();
    const auto* created = seq ? seq->find_note(note_id) : nullptr;
    if (!created) {
        return;
    }
    history_.push(std::make_unique<AddNoteCommand>(*created));
    emit undoRedoChanged();
}

void EditorController::commitTransformGesture(std::vector<NoteDelta> moves,
                                              std::vector<ResizeDelta> resizes) {
    auto compound = std::make_unique<CompoundCommand>();
    if (!moves.empty()) {
        compound->add_command(
            std::make_unique<MoveNotesCommand>(std::move(moves)));
    }
    if (!resizes.empty()) {
        compound->add_command(
            std::make_unique<ResizeNotesCommand>(std::move(resizes)));
    }
    if (!compound->empty()) {
        history_.push(std::move(compound));
        emit undoRedoChanged();
    }
}

bool EditorController::moveNote(uint64_t note_id, double new_start_beat, double new_pitch) {
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }
    auto* n = seq->find_note(note_id);
    if (!n) {
        return false;
    }

    const auto start_ticks = static_cast<int64_t>(std::llround(new_start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    if (start_ticks < 0) {
        return false;
    }

    NoteDelta delta{
        .note_id = note_id,
        .old_start = n->start,
        .old_pitch = n->pitch,
        .new_start = time::BeatPosition::from_ticks(start_ticks),
        .new_pitch = new_pitch
    };

    n->start = delta.new_start;
    n->pitch = delta.new_pitch;

    history_.push(std::make_unique<MoveNotesCommand>(std::vector<NoteDelta>{delta}));
    emit undoRedoChanged();
    syncPatternToEngine();
    emit notesChanged();
    return true;
}

bool EditorController::resizeNote(uint64_t note_id, double new_duration_beats) {
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }
    auto* n = seq->find_note(note_id);
    if (!n) {
        return false;
    }

    const auto dur_ticks = static_cast<int64_t>(std::llround(new_duration_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    if (dur_ticks <= 0) {
        return false;
    }

    ResizeDelta delta{
        .note_id = note_id,
        .old_duration = n->duration,
        .new_duration = time::BeatDuration::from_ticks(dur_ticks)
    };

    n->duration = delta.new_duration;

    history_.push(std::make_unique<ResizeNotesCommand>(std::vector<ResizeDelta>{delta}));
    emit undoRedoChanged();
    syncPatternToEngine();
    emit notesChanged();
    return true;
}

bool EditorController::removeNote(uint64_t note_id) {
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }
    const auto* n = seq->find_note(note_id);
    if (!n) {
        return false;
    }

    model::Note copy = *n;
    if (seq->remove_note(note_id)) {
        selected_note_ids_.erase(note_id);
        history_.push(std::make_unique<RemoveNotesCommand>(std::vector<model::Note>{copy}));
        emit undoRedoChanged();
        emit selectionChanged();
        syncPatternToEngine();
        emit notesChanged();
        return true;
    }
    return false;
}

bool EditorController::setNoteVelocity(uint64_t note_id, float velocity) {
    auto* seq = active_sequence();
    if (!seq) {
        return false;
    }
    auto* n = seq->find_note(note_id);
    if (!n) {
        return false;
    }

    const float clamped = std::clamp(velocity, 0.0f, 1.0f);
    VelocityDelta delta{
        .note_id = note_id,
        .old_velocity = n->velocity,
        .new_velocity = clamped
    };

    n->velocity = clamped;
    history_.push(std::make_unique<VelocityCommand>(std::vector<VelocityDelta>{delta}));
    emit undoRedoChanged();
    syncPatternToEngine();
    emit notesChanged();
    return true;
}

void EditorController::selectNote(uint64_t note_id, bool add_to_selection) {
    if (!add_to_selection) {
        selected_note_ids_.clear();
    }
    selected_note_ids_.insert(note_id);
    emit selectionChanged();
    emit notesChanged();
}

void EditorController::deselectNote(uint64_t note_id) {
    selected_note_ids_.erase(note_id);
    emit selectionChanged();
    emit notesChanged();
}

void EditorController::clearSelection() {
    if (!selected_note_ids_.empty()) {
        selected_note_ids_.clear();
        emit selectionChanged();
        emit notesChanged();
    }
}

void EditorController::selectAll() {
    auto* seq = active_sequence();
    if (!seq) {
        return;
    }
    selected_note_ids_.clear();
    for (const auto& n : seq->notes()) {
        selected_note_ids_.insert(n.note_id);
    }
    emit selectionChanged();
    emit notesChanged();
}

bool EditorController::isNoteSelected(uint64_t note_id) const noexcept {
    return selected_note_ids_.find(note_id) != selected_note_ids_.end();
}

void EditorController::deleteSelected() {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    std::vector<model::Note> removed;
    removed.reserve(selected_note_ids_.size());
    for (auto id : selected_note_ids_) {
        const auto* n = seq->find_note(id);
        if (n) {
            removed.push_back(*n);
        }
    }

    for (const auto& n : removed) {
        seq->remove_note(n.note_id);
    }
    selected_note_ids_.clear();

    if (!removed.empty()) {
        history_.push(std::make_unique<RemoveNotesCommand>(std::move(removed)));
        emit undoRedoChanged();
    }
    emit selectionChanged();
    syncPatternToEngine();
    emit notesChanged();
}

void EditorController::setSelectedVelocity(float velocity) {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    const float clamped = std::clamp(velocity, 0.0f, 1.0f);
    std::vector<VelocityDelta> deltas;
    deltas.reserve(selected_note_ids_.size());

    for (auto id : selected_note_ids_) {
        auto* n = seq->find_note(id);
        if (n) {
            deltas.push_back(VelocityDelta{
                .note_id = id,
                .old_velocity = n->velocity,
                .new_velocity = clamped
            });
            n->velocity = clamped;
        }
    }

    if (!deltas.empty()) {
        history_.push(std::make_unique<VelocityCommand>(std::move(deltas)));
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::moveSelected(double delta_beats, int delta_pitch) {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    const int64_t d_ticks = static_cast<int64_t>(std::llround(delta_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    std::vector<NoteDelta> deltas;
    deltas.reserve(selected_note_ids_.size());

    for (auto id : selected_note_ids_) {
        auto* n = seq->find_note(id);
        if (n) {
            const int64_t new_ticks = std::max<int64_t>(0, n->start.ticks + d_ticks);
            const double new_pitch = std::clamp<double>(std::round(n->pitch + delta_pitch), 0.0, 127.0);

            deltas.push_back(NoteDelta{
                .note_id = id,
                .old_start = n->start,
                .old_pitch = n->pitch,
                .new_start = time::BeatPosition::from_ticks(new_ticks),
                .new_pitch = new_pitch
            });

            n->start = time::BeatPosition::from_ticks(new_ticks);
            n->pitch = new_pitch;
        }
    }

    if (!deltas.empty()) {
        history_.push(std::make_unique<MoveNotesCommand>(std::move(deltas)));
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::resizeSelected(double delta_beats) {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    const int64_t d_ticks = static_cast<int64_t>(std::llround(delta_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    std::vector<ResizeDelta> deltas;
    deltas.reserve(selected_note_ids_.size());

    for (auto id : selected_note_ids_) {
        auto* n = seq->find_note(id);
        if (n) {
            const int64_t new_dur_ticks = std::max<int64_t>(time::BeatPosition::kTicksPerBeat / 32, n->duration.ticks + d_ticks);

            deltas.push_back(ResizeDelta{
                .note_id = id,
                .old_duration = n->duration,
                .new_duration = time::BeatDuration::from_ticks(new_dur_ticks)
            });

            n->duration = time::BeatDuration::from_ticks(new_dur_ticks);
        }
    }

    if (!deltas.empty()) {
        history_.push(std::make_unique<ResizeNotesCommand>(std::move(deltas)));
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::nudgePitchSelected(int semitones) {
    moveSelected(0.0, semitones);
}

void EditorController::nudgeBeatSelected(int grid_steps) {
    const double step_beats = snapDuration().to_double();
    moveSelected(step_beats * grid_steps, 0);
}

void EditorController::beginVelocityGesture(uint64_t note_id) {
    velocity_gesture_.clear();
    auto* seq = active_sequence();
    if (!seq) {
        return;
    }
    if (isNoteSelected(note_id)) {
        for (const auto id : selected_note_ids_) {
            if (const auto* note = seq->find_note(id)) {
                velocity_gesture_.push_back(
                    VelocityDelta{id, note->velocity, note->velocity});
            }
        }
    } else if (const auto* note = seq->find_note(note_id)) {
        velocity_gesture_.push_back(
            VelocityDelta{note_id, note->velocity, note->velocity});
    }
}

void EditorController::previewVelocityGesture(float velocity) {
    auto* seq = active_sequence();
    if (!seq || velocity_gesture_.empty()) {
        return;
    }
    const float clamped = std::clamp(velocity, 0.0f, 1.0f);
    for (auto& delta : velocity_gesture_) {
        if (auto* note = seq->find_note(delta.note_id)) {
            note->velocity = clamped;
            delta.new_velocity = clamped;
        }
    }
    syncPatternToEngine();
    emit notesChanged();
}

void EditorController::commitVelocityGesture() {
    if (velocity_gesture_.empty()) {
        return;
    }
    const bool changed = std::any_of(
        velocity_gesture_.begin(), velocity_gesture_.end(),
        [](const VelocityDelta& delta) {
            return std::abs(delta.old_velocity - delta.new_velocity) > 0.0001f;
        });
    if (changed) {
        history_.push(std::make_unique<VelocityCommand>(
            std::move(velocity_gesture_)));
        emit undoRedoChanged();
    }
    velocity_gesture_.clear();
}

void EditorController::copy() {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    clipboard_.clear();
    for (auto id : selected_note_ids_) {
        const auto* n = seq->find_note(id);
        if (n) {
            clipboard_.push_back(*n);
        }
    }
}

void EditorController::paste(double target_beat) {
    auto* seq = active_sequence();
    if (!seq || clipboard_.empty()) {
        return;
    }

    // Find earliest start in clipboard
    int64_t min_start_ticks = INT64_MAX;
    for (const auto& n : clipboard_) {
        if (n.start.ticks < min_start_ticks) {
            min_start_ticks = n.start.ticks;
        }
    }

    // Target beat defaults to current playhead
    if (target_beat < 0.0) {
        target_beat = current_beat_;
    }

    // Snap target beat
    const auto snapped_target = Coordinates::floor_quantize_beat(
        time::BeatPosition::from_fraction(static_cast<int64_t>(target_beat * 1000.0), 1000),
        snapDuration()
    );

    selected_note_ids_.clear();
    std::vector<model::Note> added_notes;
    added_notes.reserve(clipboard_.size());

    for (const auto& orig : clipboard_) {
        const int64_t offset_ticks = orig.start.ticks - min_start_ticks;
        const auto new_start = time::BeatPosition::from_ticks(snapped_target.ticks + offset_ticks);

        const auto new_id_opt = seq->add_note(new_start, orig.duration, orig.pitch, orig.velocity);
        if (new_id_opt) {
            selected_note_ids_.insert(*new_id_opt);
            const auto* created = seq->find_note(*new_id_opt);
            if (created) {
                added_notes.push_back(*created);
            }
        }
    }

    if (!added_notes.empty()) {
        auto compound = std::make_unique<CompoundCommand>();
        for (const auto& n : added_notes) {
            compound->add_command(std::make_unique<AddNoteCommand>(n));
        }
        history_.push(std::move(compound));
        emit undoRedoChanged();
    }

    emit selectionChanged();
    syncPatternToEngine();
    emit notesChanged();
}

void EditorController::duplicate() {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    // Find min start and max end of selected group
    int64_t min_start = INT64_MAX;
    int64_t max_end = 0;
    std::vector<model::Note> to_duplicate;
    to_duplicate.reserve(selected_note_ids_.size());

    for (auto id : selected_note_ids_) {
        const auto* n = seq->find_note(id);
        if (n) {
            to_duplicate.push_back(*n);
            if (n->start.ticks < min_start) min_start = n->start.ticks;
            const int64_t end = n->start.ticks + n->duration.ticks;
            if (end > max_end) max_end = end;
        }
    }

    if (to_duplicate.empty()) {
        return;
    }

    const int64_t span_ticks = max_end - min_start;
    selected_note_ids_.clear();
    std::vector<model::Note> added_notes;
    added_notes.reserve(to_duplicate.size());

    for (const auto& orig : to_duplicate) {
        const auto new_start = time::BeatPosition::from_ticks(orig.start.ticks + span_ticks);
        const auto new_id_opt = seq->add_note(new_start, orig.duration, orig.pitch, orig.velocity);
        if (new_id_opt) {
            selected_note_ids_.insert(*new_id_opt);
            const auto* created = seq->find_note(*new_id_opt);
            if (created) {
                added_notes.push_back(*created);
            }
        }
    }

    if (!added_notes.empty()) {
        auto compound = std::make_unique<CompoundCommand>();
        for (const auto& n : added_notes) {
            compound->add_command(std::make_unique<AddNoteCommand>(n));
        }
        history_.push(std::move(compound));
        emit undoRedoChanged();
    }

    emit selectionChanged();
    syncPatternToEngine();
    emit notesChanged();
}

void EditorController::undo() {
    auto* seq = active_sequence();
    if (seq && history_.undo(*seq)) {
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::redo() {
    auto* seq = active_sequence();
    if (seq && history_.redo(*seq)) {
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::beginBatch() {
    history_.begin_batch();
}

void EditorController::commitBatch() {
    history_.commit_batch();
    emit undoRedoChanged();
}

void EditorController::discardBatch() {
    history_.discard_batch();
    emit undoRedoChanged();
}

void EditorController::play() {
    if (isPlaying()) {
        return;
    }

    syncPatternToEngine(false);

    // If loop is enabled and current position is past loop end, wrap to loop start
    if (loopEnabled() && (current_beat_ < loop_start_beat_ || current_beat_ >= loop_end_beat_)) {
        seekBeats(loop_start_beat_);
    }

    engine_.play();
    is_playing_ = true;
    emit isPlayingChanged(true);
}

void EditorController::stop() {
    engine_.stop();
    is_playing_ = false;
    emit isPlayingChanged(false);
}

void EditorController::seekBeats(double beat) {
    if (beat < 0.0) {
        beat = 0.0;
    }
    const auto target_ticks = static_cast<int64_t>(std::llround(beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    engine_.seek_beats(time::BeatPosition::from_ticks(target_ticks), sample_rate_);
    current_beat_ = beat;
    emit currentBeatChanged(current_beat_);
}

void EditorController::setBpm(double new_bpm) {
    const double clamped = std::clamp(new_bpm, 20.0, 400.0);
    if (qFuzzyCompare(bpm(), clamped)) {
        return;
    }

    engine_.transport().set_bpm(clamped);
    project_.set_bpm(clamped);
    // Update loop range samples on engine
    setLoopRange(loop_start_beat_, loop_end_beat_);
    syncPatternToEngine();
    emit bpmChanged(clamped);
}

void EditorController::setLoopEnabled(bool enabled) {
    if (loopEnabled() == enabled) {
        return;
    }
    engine_.transport().set_loop_enabled(enabled);
    emit loopEnabledChanged(enabled);
}

void EditorController::setLoopRange(double start_beat, double end_beat) {
    if (start_beat < 0.0) start_beat = 0.0;
    if (end_beat <= start_beat) end_beat = start_beat + 1.0;

    loop_start_beat_ = start_beat;
    loop_end_beat_ = end_beat;

    const auto s_ticks = static_cast<int64_t>(std::llround(start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    const auto e_ticks = static_cast<int64_t>(std::llround(end_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
    engine_.transport().set_loop_range_beats(
        time::BeatPosition::from_ticks(s_ticks),
        time::BeatPosition::from_ticks(e_ticks),
        sample_rate_
    );
    emit loopRangeChanged();
}

void EditorController::setLoopStartBeat(double start_beat) {
    setLoopRange(start_beat, loop_end_beat_);
}

void EditorController::setLoopEndBeat(double end_beat) {
    setLoopRange(loop_start_beat_, end_beat);
}

void EditorController::setMetronomeEnabled(bool enabled) {
    if (metronomeEnabled() == enabled) {
        return;
    }
    engine_.set_metronome_enabled(enabled);
    emit metronomeEnabledChanged(enabled);
}

void EditorController::setMetronomeVolume(float vol) {
    engine_.set_metronome_volume(vol);
    emit metronomeVolumeChanged(vol);
}

void EditorController::setSnapStep(const QString& step) {
    if (snap_step_ == step) {
        return;
    }
    snap_step_ = step;
    emit snapStepChanged(step);
}

void EditorController::auditionNoteOn(double pitch, float velocity) {
    engine_.audition_note_on(pitch, velocity);
}

void EditorController::auditionNoteOff() {
    engine_.audition_note_off();
}

void EditorController::syncPatternToEngine(bool mark_dirty) {
    auto compiled_events = compile_project_events();
    engine_.set_track_events(compiled_events);
    if (mark_dirty) {
        set_project_dirty(true);
    }
}

std::vector<events::TimelineEvent> EditorController::compile_project_events() const {
    std::vector<events::TimelineEvent> compiled_events;
    const bool any_solo = std::any_of(
        project_.tracks().begin(), project_.tracks().end(),
        [](const model::Track& track) { return track.mixer.solo; });
    for (const auto& clip : project_.clips()) {
        const auto* source = project_.find_pattern(clip.pattern_id);
        const auto* track = project_.find_track(clip.track_id);
        if (!source || !track || track->mixer.muted ||
            (any_solo && !track->mixer.solo)) {
            continue;
        }
        const auto pattern_ticks = source->length().ticks;
        if (pattern_ticks <= 0) {
            continue;
        }
        const auto clip_end = clip.start + clip.duration;
        const auto clip_end_sample = engine_.transport().tempo_map().beat_to_sample(
            clip_end, sample_rate_);
        const float track_gain = std::pow(10.0f, track->mixer.gain_db / 20.0f);
        const float track_pan = std::clamp(track->mixer.pan, -1.0f, 1.0f);
        uint64_t repeat_index = 0;
        for (int64_t offset_ticks = 0; offset_ticks < clip.duration.ticks;
             offset_ticks += pattern_ticks, ++repeat_index) {
            const auto repeat_start = clip.start +
                time::BeatDuration::from_ticks(offset_ticks);
            auto repeated = model::PatternCompiler::compile(
                *source, engine_.transport().tempo_map(), sample_rate_, repeat_start);
            for (auto& event : repeated) {
                if (event.sample_position >= clip_end_sample) {
                    continue;
                }
                const bool canonical_instance =
                    !project_.clips().empty() &&
                    clip.id == project_.clips().front().id &&
                    repeat_index == 0;
                const uint64_t instance_bits = canonical_instance ? 0 :
                    (clip.id * 0x9E3779B185EBCA87ULL) ^
                    (repeat_index * 0xC2B2AE3D27D4EB4FULL);
                std::visit([instance_bits, track_gain, track_pan](auto& payload) {
                    payload.note_id ^= instance_bits;
                    if (payload.note_id == 0) {
                        payload.note_id = instance_bits | 1ULL;
                    }
                    using Payload = std::decay_t<decltype(payload)>;
                    if constexpr (std::is_same_v<Payload, events::NoteOn>) {
                        payload.gain = track_gain;
                        payload.pan = track_pan;
                    }
                }, event.payload);
                compiled_events.push_back(event);
            }
        }
    }
    std::sort(compiled_events.begin(), compiled_events.end());
    return compiled_events;
}

QVariantList EditorController::getNotesData() const {
    QVariantList list;
    const auto* seq = active_sequence();
    if (!seq) {
        return list;
    }

    list.reserve(static_cast<qsizetype>(seq->size()));
    for (const auto& n : seq->notes()) {
        QVariantMap m;
        m["id"] = static_cast<qulonglong>(n.note_id);
        m["startBeat"] = n.start.to_double();
        m["durationBeats"] = n.duration.to_double();
        m["pitch"] = n.pitch;
        m["velocity"] = n.velocity;
        m["isSelected"] = isNoteSelected(n.note_id);
        list.append(m);
    }
    return list;
}

QVariantMap EditorController::getVelocityStats() const {
    QVariantMap map;
    map["min"] = 0;
    map["avg"] = 96;
    map["max"] = 127;

    const auto* seq = active_sequence();
    if (!seq || seq->empty()) {
        return map;
    }

    int min_v = 127;
    int max_v = 0;
    int sum = 0;

    for (const auto& n : seq->notes()) {
        const int v = std::clamp(static_cast<int>(std::round(n.velocity * 127.0f)), 0, 127);
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum += v;
    }

    map["min"] = min_v;
    map["avg"] = static_cast<int>(static_cast<size_t>(sum) / seq->size());
    map["max"] = max_v;
    return map;
}

void EditorController::newProject() {
    stop();
    reset_project_model();
    selected_note_ids_.clear();
    clipboard_.clear();
    history_.clear();
    arrangement_undo_.clear();
    arrangement_redo_.clear();
    current_beat_ = 0.0;
    project_path_.clear();
    set_last_error({});
    engine_.transport().set_bpm(project_.bpm());
    engine_.set_master_gain_db(project_.master_gain_db());
    rebuild_synth_plan();
    setLoopRange(0.0, 16.0);
    syncPatternToEngine(false);
    set_project_dirty(false);
    emit projectPathChanged(project_path_);
    emit selectionChanged();
    emit notesChanged();
    emit undoRedoChanged();
    emit bpmChanged(project_.bpm());
    emit currentBeatChanged(current_beat_);
    remove_recovery_file();
}

bool EditorController::openProject(const QString& path) {
    const QUrl url(path);
    const QString local_path = url.isLocalFile() ? url.toLocalFile() : path;
    model::Project loaded;
    QString error;
    if (!ProjectFile::load(local_path, loaded, &error)) {
        set_last_error(error);
        return false;
    }

    stop();
    project_ = std::move(loaded);
    active_track_id_ = project_.tracks().front().id;
    active_pattern_id_ = project_.patterns().front().id();
    const auto* active = active_pattern();
    if (!active || active->lanes().empty()) {
        set_last_error(QStringLiteral("Project pattern has no note lane"));
        return false;
    }
    active_lane_id_ = active->lanes().front().id();
    selected_note_ids_.clear();
    clipboard_.clear();
    history_.clear();
    arrangement_undo_.clear();
    arrangement_redo_.clear();
    project_path_ = QFileInfo(local_path).absoluteFilePath();
    current_beat_ = 0.0;
    engine_.transport().set_bpm(project_.bpm());
    engine_.set_master_gain_db(project_.master_gain_db());
    rebuild_synth_plan();
    setLoopRange(0.0, std::max(4.0, project_.end_beat().to_double()));
    syncPatternToEngine(false);
    set_project_dirty(false);
    set_last_error({});
    emit projectPathChanged(project_path_);
    emit selectionChanged();
    emit notesChanged();
    emit undoRedoChanged();
    emit bpmChanged(project_.bpm());
    emit currentBeatChanged(current_beat_);
    if (QFileInfo(local_path).absoluteFilePath() !=
        QFileInfo(recovery_path_).absoluteFilePath()) {
        remove_recovery_file();
    }
    return true;
}

bool EditorController::saveProject() {
    if (project_path_.isEmpty()) {
        set_last_error(QStringLiteral("Choose a project file with Save As"));
        return false;
    }
    return saveProjectAs(project_path_);
}

bool EditorController::saveProjectAs(const QString& path) {
    const QUrl url(path);
    QString local_path = url.isLocalFile() ? url.toLocalFile() : path;
    if (!local_path.endsWith(".dawproj", Qt::CaseInsensitive)) {
        local_path += ".dawproj";
    }
    QString error;
    if (!ProjectFile::save(project_, local_path, &error)) {
        set_last_error(error);
        return false;
    }
    project_path_ = QFileInfo(local_path).absoluteFilePath();
    set_project_dirty(false);
    set_last_error({});
    remove_recovery_file();
    emit projectPathChanged(project_path_);
    return true;
}

bool EditorController::importMidi(const QString& path, double start_beat) {
    const QUrl url(path);
    const QString local_path = url.isLocalFile() ? url.toLocalFile() : path;
    QFile file(local_path);
    if (!file.open(QIODevice::ReadOnly)) {
        set_last_error(file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    std::string parse_error;
    const auto imported = model::import_standard_midi(
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(bytes.constData()),
            static_cast<size_t>(bytes.size())),
        &parse_error);
    if (!imported) {
        set_last_error(QString::fromStdString(parse_error));
        return false;
    }

    const auto start = time::BeatPosition::from_ticks(static_cast<int64_t>(
        std::llround(std::max(0.0, start_beat) *
                     static_cast<double>(time::BeatPosition::kTicksPerBeat))));
    const QString base_name = QFileInfo(local_path).completeBaseName();
    bool activated = false;
    for (auto imported_track : imported->tracks) {
        const QString track_name = imported_track.name.empty()
            ? base_name : QString::fromStdString(imported_track.name);
        const auto track_id = project_.add_track(track_name.toStdString());
        const auto pattern_id = project_.add_pattern(
            track_name.toStdString(), imported->length);
        auto* pattern = project_.find_pattern(pattern_id);
        model::PatternLane lane(1, "MIDI");
        lane.notes() = std::move(imported_track.notes);
        if (!pattern->add_lane(std::move(lane))) {
            set_last_error(QStringLiteral("Could not create imported MIDI lane"));
            return false;
        }
        const auto clip_id = project_.add_clip(
            track_id, pattern_id, start, imported->length);
        if (clip_id == 0) {
            set_last_error(QStringLiteral("Could not place imported MIDI clip"));
            return false;
        }
        if (!activated) {
            active_track_id_ = track_id;
            active_pattern_id_ = pattern_id;
            active_lane_id_ = pattern->lanes().front().id();
            selected_clip_id_ = clip_id;
            activated = true;
        }
    }

    if (imported->initial_bpm &&
        *imported->initial_bpm >= 20.0 && *imported->initial_bpm <= 400.0) {
        setBpm(*imported->initial_bpm);
    } else {
        syncPatternToEngine();
    }
    selected_note_ids_.clear();
    history_.clear();
    set_last_error({});
    emit arrangementChanged();
    emit notesChanged();
    emit selectionChanged();
    emit undoRedoChanged();
    return true;
}

bool EditorController::exportWav(const QString& path) {
    if (export_in_progress_) {
        return false;
    }
    const QUrl url(path);
    QString local_path = url.isLocalFile() ? url.toLocalFile() : path;
    if (!local_path.endsWith(".wav", Qt::CaseInsensitive)) {
        local_path += ".wav";
    }

    const auto start = loopEnabled()
        ? time::BeatPosition::from_ticks(static_cast<int64_t>(std::llround(
              loop_start_beat_ * static_cast<double>(time::BeatPosition::kTicksPerBeat))))
        : time::BeatPosition::zero();
    auto end = loopEnabled()
        ? time::BeatPosition::from_ticks(static_cast<int64_t>(std::llround(
              loop_end_beat_ * static_cast<double>(time::BeatPosition::kTicksPerBeat))))
        : project_.end_beat();
    if (end <= start) {
        end = start + time::BeatDuration::from_beats(4);
    }

    stop();
    export_cancel_requested_.store(false, std::memory_order_release);
    export_in_progress_ = true;
    export_progress_ = 0.0;
    emit exportStateChanged(true);
    emit exportProgressChanged(0.0);

    const auto events = compile_project_events();
    QString error;
    const bool result = ProjectExporter::render_wav(
        engine_.plan(), events, project_.bpm(), project_.master_gain_db(),
        start, end, local_path,
        &export_cancel_requested_,
        [this](double progress) {
            export_progress_ = progress;
            emit exportProgressChanged(progress);
            QCoreApplication::processEvents(QEventLoop::AllEvents);
        },
        &error);

    export_in_progress_ = false;
    emit exportStateChanged(false);
    if (!result) {
        set_last_error(error);
        return false;
    }
    set_last_error({});
    return true;
}

void EditorController::cancelExport() {
    export_cancel_requested_.store(true, std::memory_order_release);
}

QVariantList EditorController::tracksData() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(project_.tracks().size()));
    int index = 0;
    for (const auto& track : project_.tracks()) {
        QVariantMap item;
        item["id"] = static_cast<qulonglong>(track.id);
        item["index"] = index++;
        item["name"] = QString::fromStdString(track.name);
        item["gainDb"] = track.mixer.gain_db;
        item["pan"] = track.mixer.pan;
        item["muted"] = track.mixer.muted;
        item["solo"] = track.mixer.solo;
        item["selected"] = track.id == active_track_id_;
        result.append(item);
    }
    return result;
}

QVariantList EditorController::clipsData() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(project_.clips().size()));
    for (const auto& clip : project_.clips()) {
        const auto* pattern = project_.find_pattern(clip.pattern_id);
        const auto track_it = std::find_if(
            project_.tracks().begin(), project_.tracks().end(),
            [&clip](const model::Track& track) { return track.id == clip.track_id; });
        if (!pattern || track_it == project_.tracks().end()) {
            continue;
        }
        QVariantMap item;
        item["id"] = static_cast<qulonglong>(clip.id);
        item["trackId"] = static_cast<qulonglong>(clip.track_id);
        item["patternId"] = static_cast<qulonglong>(clip.pattern_id);
        item["trackIndex"] = static_cast<int>(
            std::distance(project_.tracks().begin(), track_it));
        item["name"] = QString::fromStdString(pattern->name());
        item["startBeat"] = clip.start.to_double();
        item["durationBeats"] = clip.duration.to_double();
        item["selected"] = clip.id == selected_clip_id_;
        result.append(item);
    }
    return result;
}

uint64_t EditorController::addTrack(const QString& name) {
    const auto id = project_.add_track(
        name.trimmed().isEmpty() ? "Instrument" : name.trimmed().toStdString());
    active_track_id_ = id;
    const model::Track created = *project_.find_track(id);
    push_arrangement_edit(
        [this, id]() { project_.remove_track(id); },
        [this, created]() { project_.add_track(created); });
    set_project_dirty(true);
    emit arrangementChanged();
    return id;
}

bool EditorController::renameTrack(uint64_t track_id, const QString& name) {
    auto* track = project_.find_track(track_id);
    const auto trimmed = name.trimmed();
    if (!track || trimmed.isEmpty()) {
        return false;
    }
    const std::string old_name = track->name;
    const std::string new_name = trimmed.toStdString();
    track->name = new_name;
    push_arrangement_edit(
        [this, track_id, old_name]() {
            if (auto* item = project_.find_track(track_id)) item->name = old_name;
        },
        [this, track_id, new_name]() {
            if (auto* item = project_.find_track(track_id)) item->name = new_name;
        });
    set_project_dirty(true);
    emit arrangementChanged();
    return true;
}

bool EditorController::deleteTrack(uint64_t track_id) {
    const auto* existing = project_.find_track(track_id);
    if (project_.tracks().size() <= 1 || !existing) {
        return false;
    }
    const model::Track removed_track = *existing;
    std::vector<model::ClipInstance> removed_clips;
    for (const auto& clip : project_.clips()) {
        if (clip.track_id == track_id) removed_clips.push_back(clip);
    }
    project_.remove_track(track_id);
    push_arrangement_edit(
        [this, removed_track, removed_clips]() {
            project_.add_track(removed_track);
            for (const auto& clip : removed_clips) project_.add_clip(clip);
        },
        [this, track_id]() { project_.remove_track(track_id); });
    if (active_track_id_ == track_id) {
        active_track_id_ = project_.tracks().front().id;
    }
    selected_clip_id_ = 0;
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

void EditorController::selectTrack(uint64_t track_id) {
    if (project_.find_track(track_id) && active_track_id_ != track_id) {
        active_track_id_ = track_id;
        emit arrangementChanged();
    }
}

uint64_t EditorController::createPattern(uint64_t track_id, double start_beat,
                                         double length_beats) {
    if (!project_.find_track(track_id) || start_beat < 0.0 || length_beats <= 0.0) {
        return 0;
    }
    const auto start = time::BeatPosition::from_ticks(static_cast<int64_t>(
        std::llround(start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat))));
    const auto length = time::BeatDuration::from_ticks(static_cast<int64_t>(
        std::llround(length_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat))));
    const auto pattern_id = project_.add_pattern(
        "Pattern " + std::to_string(project_.patterns().size() + 1),
        length);
    auto* pattern = project_.find_pattern(pattern_id);
    const auto lane_id = pattern->add_lane("Synth");
    const auto clip_id = project_.add_clip(
        track_id, pattern_id, start, length);
    active_track_id_ = track_id;
    active_pattern_id_ = pattern_id;
    active_lane_id_ = lane_id;
    selected_clip_id_ = clip_id;
    selected_note_ids_.clear();
    history_.clear();
    const model::Pattern created_pattern = *pattern;
    const model::ClipInstance created_clip = *project_.find_clip(clip_id);
    push_arrangement_edit(
        [this, pattern_id]() { project_.remove_pattern(pattern_id); },
        [this, created_pattern, created_clip]() {
            project_.add_pattern(created_pattern);
            project_.add_clip(created_clip);
        });
    syncPatternToEngine();
    emit arrangementChanged();
    emit notesChanged();
    emit selectionChanged();
    emit undoRedoChanged();
    return clip_id;
}

bool EditorController::openClip(uint64_t clip_id) {
    const auto* clip = project_.find_clip(clip_id);
    auto* pattern = clip ? project_.find_pattern(clip->pattern_id) : nullptr;
    if (!clip || !pattern || pattern->lanes().empty()) {
        return false;
    }
    active_track_id_ = clip->track_id;
    active_pattern_id_ = clip->pattern_id;
    active_lane_id_ = pattern->lanes().front().id();
    selected_clip_id_ = clip_id;
    selected_note_ids_.clear();
    history_.clear();
    emit arrangementChanged();
    emit notesChanged();
    emit selectionChanged();
    emit undoRedoChanged();
    return true;
}

bool EditorController::moveClip(uint64_t clip_id, double start_beat,
                                uint64_t track_id) {
    auto* clip = project_.find_clip(clip_id);
    if (!clip || !project_.find_track(track_id) || start_beat < 0.0) {
        return false;
    }
    const auto old_start = clip->start;
    const auto old_track = clip->track_id;
    const auto new_start = time::BeatPosition::from_ticks(static_cast<int64_t>(
        std::llround(start_beat * static_cast<double>(time::BeatPosition::kTicksPerBeat))));
    clip->start = new_start;
    clip->track_id = track_id;
    push_arrangement_edit(
        [this, clip_id, old_start, old_track]() {
            if (auto* item = project_.find_clip(clip_id)) {
                item->start = old_start;
                item->track_id = old_track;
            }
        },
        [this, clip_id, new_start, track_id]() {
            if (auto* item = project_.find_clip(clip_id)) {
                item->start = new_start;
                item->track_id = track_id;
            }
        });
    active_track_id_ = track_id;
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

bool EditorController::resizeClip(uint64_t clip_id, double duration_beats) {
    auto* clip = project_.find_clip(clip_id);
    if (!clip || duration_beats <= 0.0) {
        return false;
    }
    const auto old_duration = clip->duration;
    const auto new_duration = time::BeatDuration::from_ticks(static_cast<int64_t>(
        std::llround(duration_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat))));
    clip->duration = new_duration;
    push_arrangement_edit(
        [this, clip_id, old_duration]() {
            if (auto* item = project_.find_clip(clip_id)) item->duration = old_duration;
        },
        [this, clip_id, new_duration]() {
            if (auto* item = project_.find_clip(clip_id)) item->duration = new_duration;
        });
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

uint64_t EditorController::duplicateClip(uint64_t clip_id) {
    const auto* source = project_.find_clip(clip_id);
    if (!source) {
        return 0;
    }
    const auto id = project_.add_clip(
        source->track_id, source->pattern_id,
        source->start + source->duration, source->duration);
    selected_clip_id_ = id;
    const model::ClipInstance duplicated = *project_.find_clip(id);
    push_arrangement_edit(
        [this, id]() { project_.remove_clip(id); },
        [this, duplicated]() { project_.add_clip(duplicated); });
    syncPatternToEngine();
    emit arrangementChanged();
    return id;
}

bool EditorController::deleteClip(uint64_t clip_id) {
    const auto* existing = project_.find_clip(clip_id);
    if (!existing) {
        return false;
    }
    const model::ClipInstance removed = *existing;
    project_.remove_clip(clip_id);
    push_arrangement_edit(
        [this, removed]() { project_.add_clip(removed); },
        [this, clip_id]() { project_.remove_clip(clip_id); });
    if (selected_clip_id_ == clip_id) {
        selected_clip_id_ = 0;
    }
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

void EditorController::selectClip(uint64_t clip_id) {
    if (project_.find_clip(clip_id) && selected_clip_id_ != clip_id) {
        selected_clip_id_ = clip_id;
        emit arrangementChanged();
    }
}

bool EditorController::setTrackGain(uint64_t track_id, float gain_db) {
    auto* track = project_.find_track(track_id);
    if (!track) {
        return false;
    }
    track->mixer.gain_db = std::clamp(gain_db, -60.0f, 12.0f);
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

bool EditorController::setTrackPan(uint64_t track_id, float pan) {
    auto* track = project_.find_track(track_id);
    if (!track) {
        return false;
    }
    track->mixer.pan = std::clamp(pan, -1.0f, 1.0f);
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

bool EditorController::setTrackMute(uint64_t track_id, bool muted) {
    auto* track = project_.find_track(track_id);
    if (!track) {
        return false;
    }
    track->mixer.muted = muted;
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

bool EditorController::setTrackSolo(uint64_t track_id, bool solo) {
    auto* track = project_.find_track(track_id);
    if (!track) {
        return false;
    }
    track->mixer.solo = solo;
    syncPatternToEngine();
    emit arrangementChanged();
    return true;
}

void EditorController::setMasterGainDb(float gain_db) {
    const float before = engine_.master_gain_db();
    engine_.set_master_gain_db(gain_db);
    const float after = engine_.master_gain_db();
    if (std::abs(before - after) > 0.001f) {
        project_.set_master_gain_db(after);
        set_project_dirty(true);
        emit masterGainChanged(after);
    }
}

void EditorController::rebuild_synth_plan() {
    const auto& patch = project_.synth_patch();
    graph::PolySynthNode synth_patch{
        .attack_seconds = patch.attack_seconds,
        .decay_seconds = patch.decay_seconds,
        .sustain = patch.sustain,
        .release_seconds = patch.release_seconds,
        .cutoff_hz = patch.cutoff_hz,
        .resonance = patch.resonance,
        .character = patch.character,
    };
    graph::GraphModel graph;
    const auto synth = graph.add_poly_synth_node(synth_patch);
    const auto gain_left = graph.add_gain_node(-9.0f);
    const auto gain_right = graph.add_gain_node(-9.0f);
    const auto output = graph.add_output_node(2);
    graph.connect(synth, graph::PolySynthNode::kPortLeft,
                  gain_left, graph::GainNode::kPortIn);
    graph.connect(synth, graph::PolySynthNode::kPortRight,
                  gain_right, graph::GainNode::kPortIn);
    graph.connect(gain_left, graph::GainNode::kPortOut,
                  output, graph::OutputNode::kPortLeft);
    graph.connect(gain_right, graph::GainNode::kPortOut,
                  output, graph::OutputNode::kPortRight);
    engine_.publish_plan(graph::GraphCompiler::compile(graph));
}

void EditorController::commit_synth_patch(model::SynthPatch patch) {
    project_.set_synth_patch(patch);
    rebuild_synth_plan();
    set_project_dirty(true);
    emit synthPatchChanged();
}

void EditorController::setSynthAttack(float value) {
    auto patch = project_.synth_patch();
    patch.attack_seconds = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthDecay(float value) {
    auto patch = project_.synth_patch();
    patch.decay_seconds = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthSustain(float value) {
    auto patch = project_.synth_patch();
    patch.sustain = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthRelease(float value) {
    auto patch = project_.synth_patch();
    patch.release_seconds = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthCutoff(float value) {
    auto patch = project_.synth_patch();
    patch.cutoff_hz = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthResonance(float value) {
    auto patch = project_.synth_patch();
    patch.resonance = value;
    commit_synth_patch(patch);
}

void EditorController::setSynthCharacter(float value) {
    auto patch = project_.synth_patch();
    patch.character = value;
    commit_synth_patch(patch);
}

void EditorController::quantizeSelected() {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    const auto q_dur = snapDuration();
    std::vector<NoteDelta> deltas;

    for (auto id : selected_note_ids_) {
        auto* n = seq->find_note(id);
        if (n) {
            const auto new_start = Coordinates::quantize_beat(n->start, q_dur);
            if (new_start != n->start) {
                deltas.push_back(NoteDelta{
                    .note_id = id,
                    .old_start = n->start,
                    .old_pitch = n->pitch,
                    .new_start = new_start,
                    .new_pitch = n->pitch
                });
                n->start = new_start;
            }
        }
    }

    if (!deltas.empty()) {
        history_.push(std::make_unique<MoveNotesCommand>(std::move(deltas)));
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::humanizeSelected() {
    auto* seq = active_sequence();
    if (!seq || selected_note_ids_.empty()) {
        return;
    }

    std::vector<VelocityDelta> deltas;
    for (auto id : selected_note_ids_) {
        auto* n = seq->find_note(id);
        if (n) {
            // Subtle velocity shift [-0.05, +0.05]
            const float shift = (static_cast<float>((id * 17) % 21) - 10.0f) * 0.005f;
            const float new_vel = std::clamp(n->velocity + shift, 0.1f, 1.0f);
            deltas.push_back(VelocityDelta{
                .note_id = id,
                .old_velocity = n->velocity,
                .new_velocity = new_vel
            });
            n->velocity = new_vel;
        }
    }

    if (!deltas.empty()) {
        history_.push(std::make_unique<VelocityCommand>(std::move(deltas)));
        emit undoRedoChanged();
        syncPatternToEngine();
        emit notesChanged();
    }
}

void EditorController::onTimerTick() {
    engine_.collect_retired();
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

    const auto telemetry = engine_.telemetry();
    const float next_left = std::clamp(telemetry.peak_left, 0.0f, 1.0f);
    const float next_right = std::clamp(telemetry.peak_right, 0.0f, 1.0f);
    const float smoothed_left =
        next_left >= meter_left_ ? next_left : meter_left_ * 0.82f;
    const float smoothed_right =
        next_right >= meter_right_ ? next_right : meter_right_ * 0.82f;
    if (std::abs(smoothed_left - meter_left_) > 0.001f ||
        std::abs(smoothed_right - meter_right_) > 0.001f) {
        meter_left_ = smoothed_left < 0.001f ? 0.0f : smoothed_left;
        meter_right_ = smoothed_right < 0.001f ? 0.0f : smoothed_right;
        emit metersChanged();
    }
}

} // namespace saudade::ui
