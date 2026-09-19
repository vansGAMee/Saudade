#include <iostream>
#include <saudade/ui/piano_roll_item.hpp>

#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGVertexColorMaterial>
#include <QSGFlatColorMaterial>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QCursor>

#include <algorithm>
#include <cmath>

namespace saudade::ui {

PianoRollItem::PianoRollItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
}

void PianoRollItem::setController(EditorController* controller) {
    if (controller_ == controller) {
        return;
    }
    if (controller_) {
        disconnect(controller_, nullptr, this, nullptr);
    }
    controller_ = controller;
    if (controller_) {
        connect(controller_, &EditorController::notesChanged, this, [this]() {
            update();
        });
        connect(controller_, &EditorController::selectionChanged, this, [this]() {
            update();
        });
        connect(controller_, &EditorController::currentBeatChanged, this, [this]() {
            update();
        });
    }
    emit controllerChanged();
    update();
}

void PianoRollItem::setActiveTool(const QString& tool) {
    if (active_tool_ == tool) {
        return;
    }
    active_tool_ = tool;
    emit activeToolChanged();
}

void PianoRollItem::setBeatWidth(float w) {
    if (qFuzzyCompare(beat_width_, w) || w <= 0.0f) return;
    beat_width_ = w;
    emit beatWidthChanged();
    update();
}

void PianoRollItem::setRowHeight(float h) {
    if (qFuzzyCompare(row_height_, h) || h <= 0.0f) return;
    row_height_ = h;
    emit rowHeightChanged();
    update();
}

void PianoRollItem::setMinPitch(int p) {
    if (min_pitch_ == p) return;
    min_pitch_ = p;
    emit pitchRangeChanged();
    update();
}

void PianoRollItem::setMaxPitch(int p) {
    if (max_pitch_ == p) return;
    max_pitch_ = p;
    emit pitchRangeChanged();
    update();
}

PianoRollItem::HitResult PianoRollItem::hitTest(float x, float y) const noexcept {
    HitResult res{};
    if (!controller_) return res;

    const auto* seq = controller_->active_sequence();
    if (!seq) return res;

    for (const auto& note : seq->notes()) {
        const float nx = Coordinates::beat_to_x(note.start, beat_width_);
        const float nw = std::max(6.0f, Coordinates::beat_to_x(note.start + note.duration, beat_width_) - nx);
        const float ny = Coordinates::pitch_to_y(note.pitch, row_height_, max_pitch_);
        const float nh = row_height_;

        if (x >= nx && x <= (nx + nw) && y >= ny && y <= (ny + nh)) {
            res.hit = true;
            res.note_id = note.note_id;
            res.pitch = note.pitch;
            res.start = note.start;
            res.duration = note.duration;
            res.is_start_resize_handle = (x <= (nx + 8.0f));
            res.is_end_resize_handle = (x >= (nx + nw - 8.0f));
            return res;
        }
    }
    return res;
}

void PianoRollItem::hoverMoveEvent(QHoverEvent* event) {
    const auto pos = event->position();
    const auto hit = hitTest(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
    if (hit.hit) {
        setCursor((hit.is_start_resize_handle || hit.is_end_resize_handle)
                      ? Qt::SizeHorCursor : Qt::SizeAllCursor);
    } else {
        if (active_tool_ == "select") {
            setCursor(Qt::ArrowCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
    }
    event->accept();
}

void PianoRollItem::mousePressEvent(QMouseEvent* event) {
    forceActiveFocus();

    if (!controller_ || controller_->isPlaying()) {
        event->accept();
        return;
    }

    const float mx = static_cast<float>(event->position().x());
    const float my = static_cast<float>(event->position().y());
    const auto hit = hitTest(mx, my);
    const bool erase_active = temporary_erase_ || active_tool_ == "eraser";

    if (event->button() == Qt::RightButton) {
        if (hit.hit) {
            if (controller_->isNoteSelected(hit.note_id)) {
                controller_->deleteSelected();
            } else {
                controller_->removeNote(hit.note_id);
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        mouse_press_pos_ = event->position();
        const bool shift = (event->modifiers() & Qt::ShiftModifier);
        const bool ctrl = (event->modifiers() & Qt::ControlModifier);

        if (erase_active) {
            drag_mode_ = DragMode::Erase;
            controller_->beginBatch();
            if (hit.hit) {
                controller_->removeNote(hit.note_id);
            }
        } else if (hit.hit) {
            if (shift) {
                if (controller_->isNoteSelected(hit.note_id)) {
                    controller_->deselectNote(hit.note_id);
                } else {
                    controller_->selectNote(hit.note_id, true);
                }
            } else if (!controller_->isNoteSelected(hit.note_id)) {
                controller_->selectNote(hit.note_id, false);
            }

            drag_mode_ = hit.is_start_resize_handle
                ? DragMode::ResizeStart
                : (hit.is_end_resize_handle ? DragMode::ResizeEnd
                                            : DragMode::Move);
            drag_note_id_ = hit.note_id;
            initial_note_start_ = hit.start;
            initial_note_pitch_ = hit.pitch;
            initial_note_duration_ = hit.duration;
            last_audition_pitch_ = hit.pitch;

            controller_->auditionNoteOn(hit.pitch, 0.8f);
            initial_selected_states_.clear();
            const auto* seq = controller_->active_sequence();
            if (seq) {
                for (auto id : controller_->selectedNoteIds()) {
                    const auto* n = seq->find_note(id);
                    if (n) {
                        initial_selected_states_.push_back({n->note_id, n->start, n->pitch, n->duration});
                    }
                }
            }
            if (initial_selected_states_.empty()) {
                initial_selected_states_.push_back({hit.note_id, hit.start, hit.pitch, hit.duration});
            }
        } else {
            // Clicked on empty space
            if (shift || ctrl || active_tool_ == "select") {
                drag_mode_ = DragMode::Marquee;
                is_marquee_active_ = true;
                marquee_rect_ = QRectF(event->position(), QSizeF(0, 0));
                if (!shift && !ctrl) {
                    controller_->clearSelection();
                }
                update();
            } else {
                // Pencil tool default: create new note
                controller_->clearSelection();
                drag_mode_ = DragMode::Create;

                const auto raw_beat = Coordinates::x_to_beat(mx, beat_width_);
                const auto start_beat = Coordinates::floor_quantize_beat(raw_beat, controller_->snapDuration());
                const int pitch = Coordinates::quantize_pitch(my, row_height_, min_pitch_, max_pitch_);
                const auto dur = controller_->snapDuration();

                drag_note_id_ = controller_->beginNoteCreation(
                    start_beat.to_double(), dur.to_double(),
                    static_cast<double>(pitch));
                if (drag_note_id_ != 0) {
                    controller_->selectNote(drag_note_id_, false);
                }

                initial_note_start_ = start_beat;
                initial_note_pitch_ = static_cast<double>(pitch);
                initial_note_duration_ = dur;
                last_audition_pitch_ = static_cast<double>(pitch);

                controller_->auditionNoteOn(static_cast<double>(pitch), 0.8f);

                initial_selected_states_.clear();
                initial_selected_states_.push_back({drag_note_id_, start_beat, static_cast<double>(pitch), dur});
            }
        }
        event->accept();
    }
}

void PianoRollItem::mouseMoveEvent(QMouseEvent* event) {
    if (!controller_ || controller_->isPlaying() || drag_mode_ == DragMode::None) {
        return;
    }

    const float mx = static_cast<float>(event->position().x());
    const float my = static_cast<float>(event->position().y());

    if (drag_mode_ == DragMode::Marquee) {
        const qreal x0 = std::min(mouse_press_pos_.x(), event->position().x());
        const qreal x1 = std::max(mouse_press_pos_.x(), event->position().x());
        const qreal y0 = std::min(mouse_press_pos_.y(), event->position().y());
        const qreal y1 = std::max(mouse_press_pos_.y(), event->position().y());
        marquee_rect_ = QRectF(x0, y0, x1 - x0, y1 - y0);
        update();
        event->accept();
        return;
    }

    if (drag_mode_ == DragMode::Erase) {
        const auto hit = hitTest(mx, my);
        if (hit.hit) {
            controller_->removeNote(hit.note_id);
        }
        event->accept();
        return;
    }

    if (drag_mode_ == DragMode::Create) {
        const auto cur_beat = Coordinates::x_to_beat(mx, beat_width_);
        if (cur_beat > initial_note_start_) {
            const auto min_duration = time::BeatDuration::from_ticks(
                time::BeatPosition::kTicksPerBeat / 32);
            auto new_dur = Coordinates::quantize_duration(
                cur_beat - initial_note_start_, controller_->snapDuration());
            if (new_dur < min_duration) {
                new_dur = min_duration;
            }
            if (auto* seq = controller_->active_sequence()) {
                if (auto* note = seq->find_note(drag_note_id_)) {
                    note->duration = new_dur;
                    controller_->syncPatternToEngine();
                    emit controller_->notesChanged();
                }
            }
        }
    } else if (drag_mode_ == DragMode::ResizeEnd) {
        const double d_beats = (mx - mouse_press_pos_.x()) / beat_width_;
        const int64_t d_ticks = static_cast<int64_t>(std::llround(d_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));

        auto* seq = controller_->active_sequence();
        if (seq) {
            for (const auto& init : initial_selected_states_) {
                auto* n = seq->find_note(init.note_id);
                if (!n) continue;
                const int64_t new_ticks = std::max<int64_t>(time::BeatPosition::kTicksPerBeat / 32, init.duration.ticks + d_ticks);
                n->duration = Coordinates::quantize_duration(time::BeatDuration::from_ticks(new_ticks), controller_->snapDuration());
            }
            controller_->syncPatternToEngine();
            emit controller_->notesChanged();
        }
    } else if (drag_mode_ == DragMode::ResizeStart) {
        const double d_beats = (mx - mouse_press_pos_.x()) / beat_width_;
        const int64_t raw_ticks = static_cast<int64_t>(std::llround(
            d_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
        const int64_t snap_ticks = controller_->snapDuration().ticks;
        const int64_t d_ticks = snap_ticks > 1
            ? static_cast<int64_t>(std::llround(
                  static_cast<double>(raw_ticks) /
                  static_cast<double>(snap_ticks))) * snap_ticks
            : raw_ticks;
        const int64_t min_ticks = time::BeatPosition::kTicksPerBeat / 32;
        auto* seq = controller_->active_sequence();
        if (seq) {
            for (const auto& init : initial_selected_states_) {
                auto* note = seq->find_note(init.note_id);
                if (!note) continue;
                const int64_t end_ticks = init.start.ticks + init.duration.ticks;
                const int64_t new_start = std::clamp(
                    init.start.ticks + d_ticks, int64_t{0},
                    end_ticks - min_ticks);
                note->start = time::BeatPosition::from_ticks(new_start);
                note->duration = time::BeatDuration::from_ticks(
                    end_ticks - new_start);
            }
            controller_->syncPatternToEngine();
            emit controller_->notesChanged();
        }
    } else if (drag_mode_ == DragMode::Move) {
        const float dx = mx - static_cast<float>(mouse_press_pos_.x());
        const float dy = my - static_cast<float>(mouse_press_pos_.y());
        const double d_beats = dx / beat_width_;
        const int d_pitch = -static_cast<int>(std::round(dy / row_height_));

        const int64_t d_ticks = static_cast<int64_t>(std::llround(d_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
        const int64_t snap_ticks = controller_->snapDuration().ticks;
        const int64_t snapped_d_ticks = snap_ticks > 0 ? ((d_ticks + snap_ticks / 2) / snap_ticks) * snap_ticks : d_ticks;

        auto* seq = controller_->active_sequence();
        if (seq) {
            for (const auto& init : initial_selected_states_) {
                auto* n = seq->find_note(init.note_id);
                if (!n) continue;
                const int64_t new_start_ticks = std::max<int64_t>(0, init.start.ticks + snapped_d_ticks);
                const double new_pitch = std::clamp(init.pitch + d_pitch, static_cast<double>(min_pitch_), static_cast<double>(max_pitch_));
                n->start = time::BeatPosition::from_ticks(new_start_ticks);
                n->pitch = new_pitch;
            }

            const double cur_dragged_pitch = std::clamp(initial_note_pitch_ + d_pitch, static_cast<double>(min_pitch_), static_cast<double>(max_pitch_));
            if (cur_dragged_pitch != last_audition_pitch_) {
                last_audition_pitch_ = cur_dragged_pitch;
                controller_->auditionNoteOn(cur_dragged_pitch, 0.8f);
            }

            controller_->syncPatternToEngine();
            emit controller_->notesChanged();
        }
    }
    event->accept();
}

void PianoRollItem::mouseReleaseEvent(QMouseEvent* event) {
    if (last_audition_pitch_ >= 0.0) {
        if (controller_) {
            controller_->auditionNoteOff();
        }
        last_audition_pitch_ = -1.0;
    }

    if (drag_mode_ == DragMode::Marquee) {
        if (controller_) {
            const bool shift = (event->modifiers() & Qt::ShiftModifier);
            const bool ctrl = (event->modifiers() & Qt::ControlModifier);
            if (!shift && !ctrl) {
                controller_->clearSelection();
            }

            const auto* seq = controller_->active_sequence();
            if (seq) {
                for (const auto& note : seq->notes()) {
                    const float nx = Coordinates::beat_to_x(note.start, beat_width_);
                    const float nw = std::max(6.0f, Coordinates::beat_to_x(note.start + note.duration, beat_width_) - nx);
                    const float ny = Coordinates::pitch_to_y(note.pitch, row_height_, max_pitch_);
                    const float nh = row_height_;
                    const QRectF noteRect(nx, ny, nw, nh);

                    if (marquee_rect_.intersects(noteRect)) {
                        controller_->selectNote(note.note_id, true);
                    }
                }
            }
        }
        is_marquee_active_ = false;
        marquee_rect_ = QRectF();
        update();
    } else if (drag_mode_ == DragMode::Create) {
        if (controller_ && drag_note_id_ != 0) {
            controller_->commitNoteCreation(drag_note_id_);
        }
    } else if (drag_mode_ == DragMode::Erase) {
        if (controller_) controller_->commitBatch();
    } else if (drag_mode_ == DragMode::Move ||
               drag_mode_ == DragMode::ResizeStart ||
               drag_mode_ == DragMode::ResizeEnd) {
        if (controller_) {
            std::vector<NoteDelta> moves;
            std::vector<ResizeDelta> resizes;
            const auto* seq = controller_->active_sequence();
            if (seq) {
                for (const auto& initial : initial_selected_states_) {
                    const auto* note = seq->find_note(initial.note_id);
                    if (!note) continue;
                    if (note->start != initial.start ||
                        note->pitch != initial.pitch) {
                        moves.push_back(NoteDelta{
                            .note_id = note->note_id,
                            .old_start = initial.start,
                            .old_pitch = initial.pitch,
                            .new_start = note->start,
                            .new_pitch = note->pitch,
                        });
                    }
                    if (note->duration != initial.duration) {
                        resizes.push_back(ResizeDelta{
                            .note_id = note->note_id,
                            .old_duration = initial.duration,
                            .new_duration = note->duration,
                        });
                    }
                }
            }
            controller_->commitTransformGesture(
                std::move(moves), std::move(resizes));
        }
    }

    drag_mode_ = DragMode::None;
    drag_note_id_ = 0;
    initial_selected_states_.clear();
    event->accept();
}

void PianoRollItem::keyPressEvent(QKeyEvent* event) {
    if (!controller_) {
        QQuickItem::keyPressEvent(event);
        return;
    }

    const auto key = event->key();
    const auto mods = event->modifiers();

    if (key == Qt::Key_E && !event->isAutoRepeat()) {
        temporary_erase_ = true;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }

    if (key == Qt::Key_Space) {
        if (controller_->isPlaying()) {
            controller_->stop();
        } else {
            controller_->play();
        }
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_A) {
        controller_->selectAll();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_C) {
        controller_->copy();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_V) {
        controller_->paste();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_D) {
        controller_->duplicate();
        event->accept();
        return;
    }

    if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        controller_->deleteSelected();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_Z) {
        controller_->redo();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_Z) {
        controller_->undo();
        event->accept();
        return;
    }

    if ((mods & Qt::ControlModifier) && key == Qt::Key_Y) {
        controller_->redo();
        event->accept();
        return;
    }

    if (key == Qt::Key_Up) {
        controller_->nudgePitchSelected((mods & Qt::ShiftModifier) ? 12 : 1);
        event->accept();
        return;
    }

    if (key == Qt::Key_Down) {
        controller_->nudgePitchSelected((mods & Qt::ShiftModifier) ? -12 : -1);
        event->accept();
        return;
    }

    if (key == Qt::Key_Left) {
        controller_->nudgeBeatSelected((mods & Qt::ShiftModifier) ? -16 : -1);
        event->accept();
        return;
    }

    if (key == Qt::Key_Right) {
        controller_->nudgeBeatSelected((mods & Qt::ShiftModifier) ? 16 : 1);
        event->accept();
        return;
    }

    QQuickItem::keyPressEvent(event);
}

void PianoRollItem::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_E && !event->isAutoRepeat()) {
        temporary_erase_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QQuickItem::keyReleaseEvent(event);
}

QSGNode* PianoRollItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*data*/) {
    auto* root = static_cast<QSGNode*>(oldNode);
    if (!root) {
        root = new QSGNode();
    }

    QSGGeometryNode* bgBlackNode = nullptr;
    QSGGeometryNode* bgWhiteNode = nullptr;
    QSGGeometryNode* gridSubdivNode = nullptr;
    QSGGeometryNode* gridBeatNode = nullptr;
    QSGGeometryNode* gridBarNode = nullptr;
    QSGGeometryNode* gridPitchNode = nullptr;
    QSGGeometryNode* gridOctaveNode = nullptr;
    QSGGeometryNode* notesUnselectedBodyNode = nullptr;
    QSGGeometryNode* notesUnselectedHandleNode = nullptr;
    QSGGeometryNode* notesSelectedBodyNode = nullptr;
    QSGGeometryNode* notesSelectedHandleNode = nullptr;
    QSGGeometryNode* marqueeFillNode = nullptr;
    QSGGeometryNode* marqueeBorderNode = nullptr;
    QSGGeometryNode* playheadNode = nullptr;

    auto createFlatNode = [](const QColor& color) {
        auto* node = new QSGGeometryNode();
        auto* mat = new QSGFlatColorMaterial();
        mat->setColor(color);
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
        node->setFlag(QSGNode::OwnsGeometry);
        return node;
    };

    if (root->childCount() == 0) {
        // 0: bgBlack (#121318)
        bgBlackNode = createFlatNode(QColor(18, 19, 24));
        root->appendChildNode(bgBlackNode);

        // 1: bgWhite (#17191F)
        bgWhiteNode = createFlatNode(QColor(23, 25, 31));
        root->appendChildNode(bgWhiteNode);

        // 2: gridSubdiv (#202229)
        gridSubdivNode = createFlatNode(QColor(32, 34, 41));
        root->appendChildNode(gridSubdivNode);

        // 3: gridBeat (#353944)
        gridBeatNode = createFlatNode(QColor(53, 57, 68));
        root->appendChildNode(gridBeatNode);

        // 4: gridBar (#4C5059)
        gridBarNode = createFlatNode(QColor(76, 80, 89));
        root->appendChildNode(gridBarNode);

        // 5: gridPitch (#24262E)
        gridPitchNode = createFlatNode(QColor(36, 38, 46));
        root->appendChildNode(gridPitchNode);

        // 6: gridOctave (#353944)
        gridOctaveNode = createFlatNode(QColor(53, 57, 68));
        root->appendChildNode(gridOctaveNode);

        // 7: unselected notes body (#8FA5BA)
        notesUnselectedBodyNode = createFlatNode(QColor(143, 165, 186));
        root->appendChildNode(notesUnselectedBodyNode);

        // 8: unselected notes handle (#B3C9DF)
        notesUnselectedHandleNode = createFlatNode(QColor(179, 201, 223));
        root->appendChildNode(notesUnselectedHandleNode);

        // 9: selected notes body (#F1EEE7 - warm cream)
        notesSelectedBodyNode = createFlatNode(QColor(241, 238, 231));
        root->appendChildNode(notesSelectedBodyNode);

        // 10: selected notes handle (#FFFFFF - pure white)
        notesSelectedHandleNode = createFlatNode(QColor(255, 255, 255));
        root->appendChildNode(notesSelectedHandleNode);

        // 11: marquee fill (translucent cream: rgba(241, 238, 231, 35))
        marqueeFillNode = createFlatNode(QColor(241, 238, 231, 35));
        root->appendChildNode(marqueeFillNode);

        // 12: marquee border (#F1EEE7)
        marqueeBorderNode = createFlatNode(QColor(241, 238, 231));
        root->appendChildNode(marqueeBorderNode);

        // 13: playhead (#D6B49A)
        playheadNode = createFlatNode(QColor(214, 180, 154));
        root->appendChildNode(playheadNode);
    } else {
        bgBlackNode = static_cast<QSGGeometryNode*>(root->childAtIndex(0));
        bgWhiteNode = static_cast<QSGGeometryNode*>(root->childAtIndex(1));
        gridSubdivNode = static_cast<QSGGeometryNode*>(root->childAtIndex(2));
        gridBeatNode = static_cast<QSGGeometryNode*>(root->childAtIndex(3));
        gridBarNode = static_cast<QSGGeometryNode*>(root->childAtIndex(4));
        gridPitchNode = static_cast<QSGGeometryNode*>(root->childAtIndex(5));
        gridOctaveNode = static_cast<QSGGeometryNode*>(root->childAtIndex(6));
        notesUnselectedBodyNode = static_cast<QSGGeometryNode*>(root->childAtIndex(7));
        notesUnselectedHandleNode = static_cast<QSGGeometryNode*>(root->childAtIndex(8));
        notesSelectedBodyNode = static_cast<QSGGeometryNode*>(root->childAtIndex(9));
        notesSelectedHandleNode = static_cast<QSGGeometryNode*>(root->childAtIndex(10));
        marqueeFillNode = static_cast<QSGGeometryNode*>(root->childAtIndex(11));
        marqueeBorderNode = static_cast<QSGGeometryNode*>(root->childAtIndex(12));
        playheadNode = static_cast<QSGGeometryNode*>(root->childAtIndex(13));
    }

    const int num_rows = std::max(1, max_pitch_ - min_pitch_ + 1);
    const float total_w = std::max(static_cast<float>(width()),
                                   controller_ ? static_cast<float>(controller_->patternLength()) * beat_width_ : 640.0f);
    const float total_h = static_cast<float>(num_rows) * row_height_;

    auto addQuad = [](QSGGeometry::Point2D* v, int& idx, float x0, float y0, float x1, float y1) {
        v[idx++].set(x0, y0);
        v[idx++].set(x1, y0);
        v[idx++].set(x0, y1);
        v[idx++].set(x1, y0);
        v[idx++].set(x1, y1);
        v[idx++].set(x0, y1);
    };

    auto addLine = [](QSGGeometry::Point2D* v, int& idx, float x0, float y0, float x1, float y1) {
        v[idx++].set(x0, y0);
        v[idx++].set(x1, y1);
    };

    // --- 1. Background Quads ---
    int count_black_rows = 0;
    int count_white_rows = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        if (Coordinates::is_black_key(pitch)) {
            count_black_rows++;
        } else {
            count_white_rows++;
        }
    }

    auto* bgBlackGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_black_rows * 6);
    bgBlackGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vBgBlack = bgBlackGeom->vertexDataAsPoint2D();

    auto* bgWhiteGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_white_rows * 6);
    bgWhiteGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vBgWhite = bgWhiteGeom->vertexDataAsPoint2D();

    int bIdx = 0, wIdx = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        const float y0 = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        const float y1 = y0 + row_height_;
        if (Coordinates::is_black_key(pitch)) {
            addQuad(vBgBlack, bIdx, 0.0f, y0, total_w, y1);
        } else {
            addQuad(vBgWhite, wIdx, 0.0f, y0, total_w, y1);
        }
    }
    bgBlackNode->setGeometry(bgBlackGeom);
    bgBlackNode->markDirty(QSGNode::DirtyGeometry);
    bgWhiteNode->setGeometry(bgWhiteGeom);
    bgWhiteNode->markDirty(QSGNode::DirtyGeometry);

    // --- 2. Grid Lines ---
    const double pattern_len = controller_ ? controller_->patternLength() : 4.0;
    const int num_subdivs = static_cast<int>(std::ceil(pattern_len * 4.0));

    int count_octave_lines = 0;
    int count_pitch_lines = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_ + 1; ++pitch) {
        if ((pitch % 12) == 0) {
            count_octave_lines++;
        } else {
            count_pitch_lines++;
        }
    }

    auto* gridOctaveGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_octave_lines * 2);
    gridOctaveGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGridOctave = gridOctaveGeom->vertexDataAsPoint2D();

    auto* gridPitchGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_pitch_lines * 2);
    gridPitchGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGridPitch = gridPitchGeom->vertexDataAsPoint2D();

    int goIdx = 0, gpIdx = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_ + 1; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        if ((pitch % 12) == 0) {
            addLine(vGridOctave, goIdx, 0.0f, y, total_w, y);
        } else {
            addLine(vGridPitch, gpIdx, 0.0f, y, total_w, y);
        }
    }
    gridOctaveNode->setGeometry(gridOctaveGeom);
    gridOctaveNode->markDirty(QSGNode::DirtyGeometry);
    gridPitchNode->setGeometry(gridPitchGeom);
    gridPitchNode->markDirty(QSGNode::DirtyGeometry);

    // Vertical beat/subdivision lines
    int count_bar_lines = 0;
    int count_beat_lines = 0;
    int count_subdiv_lines = 0;
    for (int i = 0; i <= num_subdivs; ++i) {
        if ((i % 16) == 0) {
            count_bar_lines++;
        } else if ((i % 4) == 0) {
            count_beat_lines++;
        } else {
            count_subdiv_lines++;
        }
    }

    auto* gridBarGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_bar_lines * 2);
    gridBarGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGridBar = gridBarGeom->vertexDataAsPoint2D();

    auto* gridBeatGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_beat_lines * 2);
    gridBeatGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGridBeat = gridBeatGeom->vertexDataAsPoint2D();

    auto* gridSubdivGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_subdiv_lines * 2);
    gridSubdivGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGridSubdiv = gridSubdivGeom->vertexDataAsPoint2D();

    int gbarIdx = 0, gbeatIdx = 0, gsubIdx = 0;
    for (int i = 0; i <= num_subdivs; ++i) {
        const float x = static_cast<float>(i) * (beat_width_ * 0.25f);
        if ((i % 16) == 0) {
            addLine(vGridBar, gbarIdx, x, 0.0f, x, total_h);
        } else if ((i % 4) == 0) {
            addLine(vGridBeat, gbeatIdx, x, 0.0f, x, total_h);
        } else {
            addLine(vGridSubdiv, gsubIdx, x, 0.0f, x, total_h);
        }
    }
    gridBarNode->setGeometry(gridBarGeom);
    gridBarNode->markDirty(QSGNode::DirtyGeometry);
    gridBeatNode->setGeometry(gridBeatGeom);
    gridBeatNode->markDirty(QSGNode::DirtyGeometry);
    gridSubdivNode->setGeometry(gridSubdivGeom);
    gridSubdivNode->markDirty(QSGNode::DirtyGeometry);

    // --- 3. Notes (Separated into Unselected and Selected) ---
    const auto* seq = controller_ ? controller_->active_sequence() : nullptr;
    size_t unselected_count = 0;
    size_t selected_count = 0;

    if (seq) {
        for (const auto& note : seq->notes()) {
            if (controller_->isNoteSelected(note.note_id)) {
                selected_count++;
            } else {
                unselected_count++;
            }
        }
    }

    auto* notesUnselBodyGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(unselected_count) * 6);
    notesUnselBodyGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesUnselBody = notesUnselBodyGeom->vertexDataAsPoint2D();

    auto* notesUnselHandleGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(unselected_count) * 6);
    notesUnselHandleGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesUnselHandle = notesUnselHandleGeom->vertexDataAsPoint2D();

    auto* notesSelBodyGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(selected_count) * 6);
    notesSelBodyGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesSelBody = notesSelBodyGeom->vertexDataAsPoint2D();

    auto* notesSelHandleGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(selected_count) * 6);
    notesSelHandleGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesSelHandle = notesSelHandleGeom->vertexDataAsPoint2D();

    int nUbIdx = 0, nUhIdx = 0, nSbIdx = 0, nShIdx = 0;
    if (seq) {
        for (const auto& note : seq->notes()) {
            const float x0 = Coordinates::beat_to_x(note.start, beat_width_);
            const float x1 = std::max(x0 + 6.0f, Coordinates::beat_to_x(note.start + note.duration, beat_width_));
            const float y0 = Coordinates::pitch_to_y(note.pitch, row_height_, max_pitch_) + 1.0f;
            const float y1 = y0 + row_height_ - 2.0f;
            const float x_handle = std::max(x0 + 2.0f, x1 - 6.0f);

            if (controller_->isNoteSelected(note.note_id)) {
                addQuad(vNotesSelBody, nSbIdx, x0, y0, x_handle, y1);
                addQuad(vNotesSelHandle, nShIdx, x_handle, y0, x1, y1);
            } else {
                addQuad(vNotesUnselBody, nUbIdx, x0, y0, x_handle, y1);
                addQuad(vNotesUnselHandle, nUhIdx, x_handle, y0, x1, y1);
            }
        }
    }
    notesUnselectedBodyNode->setGeometry(notesUnselBodyGeom);
    notesUnselectedBodyNode->markDirty(QSGNode::DirtyGeometry);
    notesUnselectedHandleNode->setGeometry(notesUnselHandleGeom);
    notesUnselectedHandleNode->markDirty(QSGNode::DirtyGeometry);

    notesSelectedBodyNode->setGeometry(notesSelBodyGeom);
    notesSelectedBodyNode->markDirty(QSGNode::DirtyGeometry);
    notesSelectedHandleNode->setGeometry(notesSelHandleGeom);
    notesSelectedHandleNode->markDirty(QSGNode::DirtyGeometry);

    // --- 4. Marquee Selection Box ---
    if (is_marquee_active_ && marquee_rect_.isValid() && marquee_rect_.width() > 1.0 && marquee_rect_.height() > 1.0) {
        auto* marqFillGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 6);
        marqFillGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        auto* vMarqFill = marqFillGeom->vertexDataAsPoint2D();
        int mfIdx = 0;
        addQuad(vMarqFill, mfIdx,
                static_cast<float>(marquee_rect_.left()), static_cast<float>(marquee_rect_.top()),
                static_cast<float>(marquee_rect_.right()), static_cast<float>(marquee_rect_.bottom()));
        marqueeFillNode->setGeometry(marqFillGeom);
        marqueeFillNode->markDirty(QSGNode::DirtyGeometry);

        auto* marqBorderGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 8);
        marqBorderGeom->setDrawingMode(QSGGeometry::DrawLines);
        marqBorderGeom->setLineWidth(1.0f);
        auto* vMarqBorder = marqBorderGeom->vertexDataAsPoint2D();
        int mbIdx = 0;
        const float mx0 = static_cast<float>(marquee_rect_.left());
        const float my0 = static_cast<float>(marquee_rect_.top());
        const float mx1 = static_cast<float>(marquee_rect_.right());
        const float my1 = static_cast<float>(marquee_rect_.bottom());
        addLine(vMarqBorder, mbIdx, mx0, my0, mx1, my0);
        addLine(vMarqBorder, mbIdx, mx1, my0, mx1, my1);
        addLine(vMarqBorder, mbIdx, mx1, my1, mx0, my1);
        addLine(vMarqBorder, mbIdx, mx0, my1, mx0, my0);
        marqueeBorderNode->setGeometry(marqBorderGeom);
        marqueeBorderNode->markDirty(QSGNode::DirtyGeometry);
    } else {
        auto* emptyGeom1 = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        marqueeFillNode->setGeometry(emptyGeom1);
        marqueeFillNode->markDirty(QSGNode::DirtyGeometry);

        auto* emptyGeom2 = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        marqueeBorderNode->setGeometry(emptyGeom2);
        marqueeBorderNode->markDirty(QSGNode::DirtyGeometry);
    }

    // --- 5. Playhead ---
    auto* playheadGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
    playheadGeom->setDrawingMode(QSGGeometry::DrawLines);
    playheadGeom->setLineWidth(2.0f);
    auto* vPlayhead = playheadGeom->vertexDataAsPoint2D();

    const float playhead_x = controller_ ? Coordinates::beat_to_x(
        time::BeatPosition::from_ticks(static_cast<int64_t>(std::llround(controller_->currentBeat() * static_cast<double>(time::BeatPosition::kTicksPerBeat)))),
        beat_width_) : 0.0f;

    vPlayhead[0].set(playhead_x, 0.0f);
    vPlayhead[1].set(playhead_x, total_h);

    playheadNode->setGeometry(playheadGeom);
    playheadNode->markDirty(QSGNode::DirtyGeometry);

    return root;
}

} // namespace saudade::ui
