#pragma once

#include <saudade/model/note_sequence.hpp>
#include <saudade/time/time_types.hpp>
#include <vector>
#include <memory>
#include <cstdint>

namespace saudade::ui {

class IEditCommand {
public:
    virtual ~IEditCommand() = default;
    virtual void undo(model::NoteSequence& seq) = 0;
    virtual void redo(model::NoteSequence& seq) = 0;
};

class AddNoteCommand : public IEditCommand {
public:
    explicit AddNoteCommand(model::Note note) : note_(note) {}
    void undo(model::NoteSequence& seq) override {
        seq.remove_note(note_.note_id);
    }
    void redo(model::NoteSequence& seq) override {
        seq.add_note(note_);
    }
    [[nodiscard]] const model::Note& note() const noexcept { return note_; }

private:
    model::Note note_;
};

class RemoveNotesCommand : public IEditCommand {
public:
    explicit RemoveNotesCommand(std::vector<model::Note> notes) : notes_(std::move(notes)) {}
    void undo(model::NoteSequence& seq) override {
        for (const auto& n : notes_) {
            seq.add_note(n);
        }
    }
    void redo(model::NoteSequence& seq) override {
        for (const auto& n : notes_) {
            seq.remove_note(n.note_id);
        }
    }

private:
    std::vector<model::Note> notes_;
};

struct NoteDelta {
    events::NoteId note_id{0};
    time::BeatPosition old_start{0};
    double old_pitch{60.0};
    time::BeatPosition new_start{0};
    double new_pitch{60.0};
};

class MoveNotesCommand : public IEditCommand {
public:
    explicit MoveNotesCommand(std::vector<NoteDelta> deltas) : deltas_(std::move(deltas)) {}
    void undo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.move_note(d.note_id, d.old_start);
            seq.update_pitch(d.note_id, d.old_pitch);
        }
    }
    void redo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.move_note(d.note_id, d.new_start);
            seq.update_pitch(d.note_id, d.new_pitch);
        }
    }

private:
    std::vector<NoteDelta> deltas_;
};

struct ResizeDelta {
    events::NoteId note_id{0};
    time::BeatDuration old_duration{0};
    time::BeatDuration new_duration{0};
};

class ResizeNotesCommand : public IEditCommand {
public:
    explicit ResizeNotesCommand(std::vector<ResizeDelta> deltas) : deltas_(std::move(deltas)) {}
    void undo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.resize_note(d.note_id, d.old_duration);
        }
    }
    void redo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.resize_note(d.note_id, d.new_duration);
        }
    }

private:
    std::vector<ResizeDelta> deltas_;
};

struct VelocityDelta {
    events::NoteId note_id{0};
    float old_velocity{0.8f};
    float new_velocity{0.8f};
};

class VelocityCommand : public IEditCommand {
public:
    explicit VelocityCommand(std::vector<VelocityDelta> deltas) : deltas_(std::move(deltas)) {}
    void undo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.update_velocity(d.note_id, d.old_velocity);
        }
    }
    void redo(model::NoteSequence& seq) override {
        for (const auto& d : deltas_) {
            seq.update_velocity(d.note_id, d.new_velocity);
        }
    }

private:
    std::vector<VelocityDelta> deltas_;
};

class CompoundCommand : public IEditCommand {
public:
    CompoundCommand() = default;
    void add_command(std::unique_ptr<IEditCommand> cmd) {
        if (cmd) {
            commands_.push_back(std::move(cmd));
        }
    }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

    void undo(model::NoteSequence& seq) override {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->undo(seq);
        }
    }
    void redo(model::NoteSequence& seq) override {
        for (auto& cmd : commands_) {
            cmd->redo(seq);
        }
    }

private:
    std::vector<std::unique_ptr<IEditCommand>> commands_;
};

class CommandHistory {
public:
    CommandHistory() = default;

    void push(std::unique_ptr<IEditCommand> cmd) {
        if (in_batch_) {
            current_batch_->add_command(std::move(cmd));
            return;
        }
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();
    }

    void begin_batch() {
        if (!in_batch_) {
            in_batch_ = true;
            current_batch_ = std::make_unique<CompoundCommand>();
        }
    }

    void commit_batch() {
        if (in_batch_ && current_batch_) {
            in_batch_ = false;
            if (!current_batch_->empty()) {
                undo_stack_.push_back(std::move(current_batch_));
                redo_stack_.clear();
            }
            current_batch_.reset();
        }
    }

    void discard_batch() {
        if (in_batch_) {
            in_batch_ = false;
            current_batch_.reset();
        }
    }

    [[nodiscard]] bool can_undo() const noexcept { return !undo_stack_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_stack_.empty(); }

    bool undo(model::NoteSequence& seq) {
        if (undo_stack_.empty()) {
            return false;
        }
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo(seq);
        redo_stack_.push_back(std::move(cmd));
        return true;
    }

    bool redo(model::NoteSequence& seq) {
        if (redo_stack_.empty()) {
            return false;
        }
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->redo(seq);
        undo_stack_.push_back(std::move(cmd));
        return true;
    }

    void clear() {
        undo_stack_.clear();
        redo_stack_.clear();
        discard_batch();
    }

private:
    std::vector<std::unique_ptr<IEditCommand>> undo_stack_;
    std::vector<std::unique_ptr<IEditCommand>> redo_stack_;
    bool in_batch_{false};
    std::unique_ptr<CompoundCommand> current_batch_;
};

} // namespace saudade::ui
