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
        connect(controller_, &EditorController::currentBeatChanged, this, [this]() {
            update();
        });
    }
    emit controllerChanged();
    update();
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
            res.is_resize_handle = (x >= (nx + nw - 8.0f));
            return res;
        }
    }
    return res;
}

void PianoRollItem::hoverMoveEvent(QHoverEvent* event) {
    const auto pos = event->position();
    const auto hit = hitTest(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
    if (hit.hit) {
        setCursor(hit.is_resize_handle ? Qt::SizeHorCursor : Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    event->accept();
}

void PianoRollItem::mousePressEvent(QMouseEvent* event) {
    if (!controller_ || controller_->isPlaying()) {
        event->accept();
        return;
    }

    const float mx = static_cast<float>(event->position().x());
    const float my = static_cast<float>(event->position().y());
    const auto hit = hitTest(mx, my);

    if (event->button() == Qt::RightButton) {
        if (hit.hit) {
            controller_->removeNote(hit.note_id);
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        mouse_press_pos_ = event->position();

        if (hit.hit) {
            drag_note_id_ = hit.note_id;
            initial_note_start_ = hit.start;
            initial_note_pitch_ = hit.pitch;
            initial_note_duration_ = hit.duration;
            drag_mode_ = hit.is_resize_handle ? DragMode::Resize : DragMode::Move;
        } else {
            // Left click on empty space: create new note
            drag_mode_ = DragMode::Create;
            const auto raw_beat = Coordinates::x_to_beat(mx, beat_width_);
            const auto start_beat = Coordinates::floor_quantize_beat(raw_beat);
            const int pitch = Coordinates::quantize_pitch(my, row_height_, min_pitch_, max_pitch_);
            const auto dur = time::BeatDuration::from_fraction(1, 4); // 1/4 beat default

            drag_note_id_ = controller_->addNote(start_beat.to_double(), dur.to_double(), pitch);

            initial_note_start_ = start_beat;
            initial_note_pitch_ = pitch;
            initial_note_duration_ = dur;
        }
        event->accept();
    }
}

void PianoRollItem::mouseMoveEvent(QMouseEvent* event) {
    if (!controller_ || controller_->isPlaying() || drag_mode_ == DragMode::None || drag_note_id_ == 0) {
        return;
    }

    const float mx = static_cast<float>(event->position().x());
    const float my = static_cast<float>(event->position().y());

    if (drag_mode_ == DragMode::Create || drag_mode_ == DragMode::Resize) {
        const auto cur_beat = Coordinates::x_to_beat(mx, beat_width_);
        if (cur_beat > initial_note_start_) {
            const auto new_dur = Coordinates::quantize_duration(cur_beat - initial_note_start_);
            controller_->resizeNote(drag_note_id_, new_dur.to_double());
        }
    } else if (drag_mode_ == DragMode::Move) {
        const float dx = mx - static_cast<float>(mouse_press_pos_.x());
        const float dy = my - static_cast<float>(mouse_press_pos_.y());
        const double d_beats = dx / beat_width_;
        const int d_pitch = -static_cast<int>(std::round(dy / row_height_));

        const int64_t d_ticks = static_cast<int64_t>(std::llround(d_beats * static_cast<double>(time::BeatPosition::kTicksPerBeat)));
        const auto target_beat = Coordinates::quantize_beat(
            time::BeatPosition::from_ticks(std::max<int64_t>(0, initial_note_start_.ticks + d_ticks)));
        const double target_pitch = std::clamp<double>(
            std::round(initial_note_pitch_ + d_pitch), min_pitch_, max_pitch_);

        controller_->moveNote(drag_note_id_, target_beat.to_double(), target_pitch);
    }
    event->accept();
}

void PianoRollItem::mouseReleaseEvent(QMouseEvent* event) {
    drag_mode_ = DragMode::None;
    drag_note_id_ = 0;
    event->accept();
}

void PianoRollItem::keyPressEvent(QKeyEvent* event) {
    QQuickItem::keyPressEvent(event);
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
    QSGGeometryNode* notesBodyNode = nullptr;
    QSGGeometryNode* notesHandleNode = nullptr;
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

        // 7: notesBody (#8FA5BA)
        notesBodyNode = createFlatNode(QColor(143, 165, 186));
        root->appendChildNode(notesBodyNode);

        // 8: notesHandle (#B3C9DF)
        notesHandleNode = createFlatNode(QColor(179, 201, 223));
        root->appendChildNode(notesHandleNode);

        // 9: playhead (#D6B49A)
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
        notesBodyNode = static_cast<QSGGeometryNode*>(root->childAtIndex(7));
        notesHandleNode = static_cast<QSGGeometryNode*>(root->childAtIndex(8));
        playheadNode = static_cast<QSGGeometryNode*>(root->childAtIndex(9));
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

    // --- 3. Notes ---
    const auto* seq = controller_ ? controller_->active_sequence() : nullptr;
    const size_t note_count = seq ? seq->size() : 0;

    auto* notesBodyGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(note_count) * 6);
    notesBodyGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesBody = notesBodyGeom->vertexDataAsPoint2D();

    auto* notesHandleGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(note_count) * 6);
    notesHandleGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotesHandle = notesHandleGeom->vertexDataAsPoint2D();

    int nbIdx = 0, nhIdx = 0;
    if (seq) {
        for (const auto& note : seq->notes()) {
            const float x0 = Coordinates::beat_to_x(note.start, beat_width_);
            const float x1 = std::max(x0 + 6.0f, Coordinates::beat_to_x(note.start + note.duration, beat_width_));
            const float y0 = Coordinates::pitch_to_y(note.pitch, row_height_, max_pitch_) + 1.0f;
            const float y1 = y0 + row_height_ - 2.0f;
            const float x_handle = std::max(x0 + 2.0f, x1 - 6.0f);

            addQuad(vNotesBody, nbIdx, x0, y0, x_handle, y1);
            addQuad(vNotesHandle, nhIdx, x_handle, y0, x1, y1);
        }
    }
    notesBodyNode->setGeometry(notesBodyGeom);
    notesBodyNode->markDirty(QSGNode::DirtyGeometry);
    notesHandleNode->setGeometry(notesHandleGeom);
    notesHandleNode->markDirty(QSGNode::DirtyGeometry);

    // --- 4. Playhead ---
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
