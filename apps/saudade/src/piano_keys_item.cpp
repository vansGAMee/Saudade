#include <saudade/ui/piano_keys_item.hpp>

#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGVertexColorMaterial>
#include <QSGFlatColorMaterial>

namespace saudade::ui {

PianoKeysItem::PianoKeysItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void PianoKeysItem::setRowHeight(float h) {
    if (qFuzzyCompare(row_height_, h)) return;
    row_height_ = h;
    emit rowHeightChanged();
    update();
}

void PianoKeysItem::setMinPitch(int p) {
    if (min_pitch_ == p) return;
    min_pitch_ = p;
    emit pitchRangeChanged();
    update();
}

void PianoKeysItem::setMaxPitch(int p) {
    if (max_pitch_ == p) return;
    max_pitch_ = p;
    emit pitchRangeChanged();
    update();
}

QSGNode* PianoKeysItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*data*/) {
    auto* root = static_cast<QSGNode*>(oldNode);
    if (!root) {
        root = new QSGNode();
    }

    QSGGeometryNode* keysNode = nullptr;
    QSGGeometryNode* linesNode = nullptr;

    if (root->childCount() == 0) {
        keysNode = new QSGGeometryNode();
        keysNode->setMaterial(new QSGVertexColorMaterial());
        keysNode->setFlag(QSGNode::OwnsMaterial);
        keysNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(keysNode);

        linesNode = new QSGGeometryNode();
        auto* lineMat = new QSGFlatColorMaterial();
        lineMat->setColor(QColor(24, 24, 30));
        linesNode->setMaterial(lineMat);
        linesNode->setFlag(QSGNode::OwnsMaterial);
        linesNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(linesNode);
    } else {
        keysNode = static_cast<QSGGeometryNode*>(root->childAtIndex(0));
        linesNode = static_cast<QSGGeometryNode*>(root->childAtIndex(1));
    }

    const int num_rows = std::max(1, max_pitch_ - min_pitch_ + 1);
    const float w = static_cast<float>(width());

    // 1. Build keys quads (6 vertices per row)
    auto* keysGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), num_rows * 6);
    keysGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vKeys = keysGeom->vertexDataAsColoredPoint2D();

    int vIdx = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        const bool is_c = ((pitch % 12) == 0);
        const bool is_black = Coordinates::is_black_key(pitch);

        unsigned char r = 40, g = 41, b = 50, a = 255;
        if (is_black) {
            r = 20; g = 21; b = 26;
        } else if (is_c) {
            r = 60; g = 64; b = 78;
        }

        const float x0 = 0.0f;
        const float x1 = is_black ? (w * 0.65f) : w;
        const float y0 = y;
        const float y1 = y + row_height_;

        vKeys[vIdx++].set(x0, y0, r, g, b, a);
        vKeys[vIdx++].set(x1, y0, r, g, b, a);
        vKeys[vIdx++].set(x0, y1, r, g, b, a);

        vKeys[vIdx++].set(x1, y0, r, g, b, a);
        vKeys[vIdx++].set(x1, y1, r, g, b, a);
        vKeys[vIdx++].set(x0, y1, r, g, b, a);
    }
    keysNode->setGeometry(keysGeom);
    keysNode->markDirty(QSGNode::DirtyGeometry);

    // 2. Build divider lines
    auto* linesGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), num_rows * 2 + 2);
    linesGeom->setDrawingMode(QSGGeometry::DrawLines);
    auto* vLines = linesGeom->vertexDataAsPoint2D();

    int lIdx = 0;
    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        vLines[lIdx++].set(0.0f, y);
        vLines[lIdx++].set(w, y);
    }
    vLines[lIdx++].set(w, 0.0f);
    vLines[lIdx++].set(w, static_cast<float>(height()));

    linesNode->setGeometry(linesGeom);
    linesNode->markDirty(QSGNode::DirtyGeometry);

    return root;
}

} // namespace saudade::ui
