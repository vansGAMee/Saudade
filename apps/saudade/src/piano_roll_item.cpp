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

    QSGGeometryNode* bgNode = nullptr;
    QSGGeometryNode* gridNode = nullptr;
    QSGGeometryNode* notesNode = nullptr;
    QSGGeometryNode* playheadNode = nullptr;

    if (root->childCount() == 0) {
        // Child 0: Background
        bgNode = new QSGGeometryNode();
        bgNode->setMaterial(new QSGVertexColorMaterial());
        bgNode->setFlag(QSGNode::OwnsMaterial);
        bgNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(bgNode);

        // Child 1: Grid
        gridNode = new QSGGeometryNode();
        gridNode->setMaterial(new QSGVertexColorMaterial());
        gridNode->setFlag(QSGNode::OwnsMaterial);
        gridNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(gridNode);

        // Child 2: Notes
        notesNode = new QSGGeometryNode();
        notesNode->setMaterial(new QSGVertexColorMaterial());
        notesNode->setFlag(QSGNode::OwnsMaterial);
        notesNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(notesNode);

        // Child 3: Playhead
        playheadNode = new QSGGeometryNode();
        auto* playheadMat = new QSGFlatColorMaterial();
        playheadMat->setColor(QColor(249, 115, 22)); // Orange
        playheadNode->setMaterial(playheadMat);
        playheadNode->setFlag(QSGNode::OwnsMaterial);
        playheadNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(playheadNode);
    } else {
        bgNode = static_cast<QSGGeometryNode*>(root->childAtIndex(0));
        gridNode = static_cast<QSGGeometryNode*>(root->childAtIndex(1));
        notesNode = static_cast<QSGGeometryNode*>(root->childAtIndex(2));
        playheadNode = static_cast<QSGGeometryNode*>(root->childAtIndex(3));
    }

    const int num_rows = std::max(1, max_pitch_ - min_pitch_ + 1);
    const float total_w = std::max(static_cast<float>(width()),
                                   controller_ ? static_cast<float>(controller_->patternLength()) * beat_width_ : 640.0f);
    const float total_h = static_cast<float>(num_rows) * row_height_;

    // --- 1. Background Quads (Alternating black/white key rows) ---
    auto* bgGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), num_rows * 6);
    bgGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vBg = bgGeom->vertexDataAsColoredPoint2D();

    int bgIdx = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        const bool is_black = Coordinates::is_black_key(pitch);

        const unsigned char r = is_black ? 18 : 24;
        const unsigned char g = is_black ? 18 : 24;
        const unsigned char b = is_black ? 22 : 29;
        const unsigned char a = 255;

        const float y0 = y;
        const float y1 = y + row_height_;

        vBg[bgIdx++].set(0.0f, y0, r, g, b, a);
        vBg[bgIdx++].set(total_w, y0, r, g, b, a);
        vBg[bgIdx++].set(0.0f, y1, r, g, b, a);

        vBg[bgIdx++].set(total_w, y0, r, g, b, a);
        vBg[bgIdx++].set(total_w, y1, r, g, b, a);
        vBg[bgIdx++].set(0.0f, y1, r, g, b, a);
    }
    bgNode->setGeometry(bgGeom);
    bgNode->markDirty(QSGNode::DirtyGeometry);

    // --- 2. Grid Lines (Horizontal row lines & Vertical 1/4 beat lines) ---
    const double pattern_len = controller_ ? controller_->patternLength() : 4.0;
    const int num_subdivs = static_cast<int>(std::ceil(pattern_len * 4.0));
    const int grid_line_count = (num_rows + 1) + (num_subdivs + 1);

    auto* gridGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), grid_line_count * 2);
    gridGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vGrid = gridGeom->vertexDataAsColoredPoint2D();

    int gIdx = 0;
    // Horizontal pitch lines
    for (int pitch = min_pitch_; pitch <= max_pitch_ + 1; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        const unsigned char r = 35, g = 36, b = 44, a = 255;
        vGrid[gIdx++].set(0.0f, y, r, g, b, a);
        vGrid[gIdx++].set(total_w, y, r, g, b, a);
    }

    // Vertical beat/subdivision lines
    for (int i = 0; i <= num_subdivs; ++i) {
        const float x = static_cast<float>(i) * (beat_width_ * 0.25f);
        const bool is_whole_beat = ((i % 4) == 0);
        const unsigned char r = is_whole_beat ? 60 : 35;
        const unsigned char g = is_whole_beat ? 62 : 36;
        const unsigned char b = is_whole_beat ? 76 : 44;
        const unsigned char a = 255;

        vGrid[gIdx++].set(x, 0.0f, r, g, b, a);
        vGrid[gIdx++].set(x, total_h, r, g, b, a);
    }
    gridNode->setGeometry(gridGeom);
    gridNode->markDirty(QSGNode::DirtyGeometry);

    // --- 3. Notes (Colored Quads) ---
    const auto* seq = controller_ ? controller_->active_sequence() : nullptr;
    const size_t note_count = seq ? seq->size() : 0;

    auto* notesGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), static_cast<int>(note_count) * 12);
    notesGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vNotes = notesGeom->vertexDataAsColoredPoint2D();

    int nIdx = 0;
    if (seq) {
        for (const auto& note : seq->notes()) {
            const float x0 = Coordinates::beat_to_x(note.start, beat_width_);
            const float x1 = std::max(x0 + 6.0f, Coordinates::beat_to_x(note.start + note.duration, beat_width_));
            const float y0 = Coordinates::pitch_to_y(note.pitch, row_height_, max_pitch_) + 1.0f;
            const float y1 = y0 + row_height_ - 2.0f;

            // Note body color: #3b82f6 (blue)
            const unsigned char br = 59, bg_c = 130, bb = 246, ba = 255;
            // Resize handle color: #93c5fd (light blue)
            const unsigned char hr = 147, hg = 197, hb = 253, ha = 255;

            const float x_handle = std::max(x0 + 2.0f, x1 - 6.0f);

            // Note Main Body Quad (x0 to x_handle)
            vNotes[nIdx++].set(x0, y0, br, bg_c, bb, ba);
            vNotes[nIdx++].set(x_handle, y0, br, bg_c, bb, ba);
            vNotes[nIdx++].set(x0, y1, br, bg_c, bb, ba);

            vNotes[nIdx++].set(x_handle, y0, br, bg_c, bb, ba);
            vNotes[nIdx++].set(x_handle, y1, br, bg_c, bb, ba);
            vNotes[nIdx++].set(x0, y1, br, bg_c, bb, ba);

            // Note Resize Handle Quad (x_handle to x1)
            vNotes[nIdx++].set(x_handle, y0, hr, hg, hb, ha);
            vNotes[nIdx++].set(x1, y0, hr, hg, hb, ha);
            vNotes[nIdx++].set(x_handle, y1, hr, hg, hb, ha);

            vNotes[nIdx++].set(x1, y0, hr, hg, hb, ha);
            vNotes[nIdx++].set(x1, y1, hr, hg, hb, ha);
            vNotes[nIdx++].set(x_handle, y1, hr, hg, hb, ha);
        }
    }
    notesNode->setGeometry(notesGeom);
    notesNode->markDirty(QSGNode::DirtyGeometry);

    // --- 4. Playhead (Vertical Line) ---
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
