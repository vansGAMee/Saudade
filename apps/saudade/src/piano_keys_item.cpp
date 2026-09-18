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

    QSGGeometryNode* whiteKeysNode = nullptr;
    QSGGeometryNode* cKeysNode = nullptr;
    QSGGeometryNode* blackKeysNode = nullptr;
    QSGGeometryNode* linesNode = nullptr;

    if (root->childCount() == 0) {
        // Child 0: White keys (#1D1F26)
        whiteKeysNode = new QSGGeometryNode();
        auto* whiteMat = new QSGFlatColorMaterial();
        whiteMat->setColor(QColor(29, 31, 38));
        whiteKeysNode->setMaterial(whiteMat);
        whiteKeysNode->setFlag(QSGNode::OwnsMaterial);
        whiteKeysNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(whiteKeysNode);

        // Child 1: C keys (#282B34)
        cKeysNode = new QSGGeometryNode();
        auto* cMat = new QSGFlatColorMaterial();
        cMat->setColor(QColor(40, 43, 52));
        cKeysNode->setMaterial(cMat);
        cKeysNode->setFlag(QSGNode::OwnsMaterial);
        cKeysNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(cKeysNode);

        // Child 2: Black keys (#0E0F13)
        blackKeysNode = new QSGGeometryNode();
        auto* blackMat = new QSGFlatColorMaterial();
        blackMat->setColor(QColor(14, 15, 19));
        blackKeysNode->setMaterial(blackMat);
        blackKeysNode->setFlag(QSGNode::OwnsMaterial);
        blackKeysNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(blackKeysNode);

        // Child 3: Divider lines (#18181E)
        linesNode = new QSGGeometryNode();
        auto* lineMat = new QSGFlatColorMaterial();
        lineMat->setColor(QColor(24, 24, 30));
        linesNode->setMaterial(lineMat);
        linesNode->setFlag(QSGNode::OwnsMaterial);
        linesNode->setFlag(QSGNode::OwnsGeometry);
        root->appendChildNode(linesNode);
    } else {
        whiteKeysNode = static_cast<QSGGeometryNode*>(root->childAtIndex(0));
        cKeysNode = static_cast<QSGGeometryNode*>(root->childAtIndex(1));
        blackKeysNode = static_cast<QSGGeometryNode*>(root->childAtIndex(2));
        linesNode = static_cast<QSGGeometryNode*>(root->childAtIndex(3));
    }

    const int num_rows = std::max(1, max_pitch_ - min_pitch_ + 1);
    const float w = static_cast<float>(width());

    int count_white = 0;
    int count_c = 0;
    int count_black = 0;

    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        if (Coordinates::is_black_key(pitch)) {
            count_black++;
        } else if ((pitch % 12) == 0) {
            count_c++;
        } else {
            count_white++;
        }
    }

    auto* whiteGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_white * 6);
    whiteGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vWhite = whiteGeom->vertexDataAsPoint2D();

    auto* cGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_c * 6);
    cGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vC = cGeom->vertexDataAsPoint2D();

    auto* blackGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), count_black * 6);
    blackGeom->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vBlack = blackGeom->vertexDataAsPoint2D();

    int wIdx = 0, cIdx = 0, bIdx = 0;

    auto addQuad = [](QSGGeometry::Point2D* v, int& idx, float x0, float y0, float x1, float y1) {
        v[idx++].set(x0, y0);
        v[idx++].set(x1, y0);
        v[idx++].set(x0, y1);
        v[idx++].set(x1, y0);
        v[idx++].set(x1, y1);
        v[idx++].set(x0, y1);
    };

    for (int pitch = min_pitch_; pitch <= max_pitch_; ++pitch) {
        const float y = Coordinates::pitch_to_y(pitch, row_height_, max_pitch_);
        const bool is_c = ((pitch % 12) == 0);
        const bool is_black = Coordinates::is_black_key(pitch);

        const float x0 = 0.0f;
        const float x1 = is_black ? (w * 0.65f) : w;
        const float y0 = y;
        const float y1 = y + row_height_;

        if (is_black) {
            addQuad(vBlack, bIdx, x0, y0, x1, y1);
        } else if (is_c) {
            addQuad(vC, cIdx, x0, y0, x1, y1);
        } else {
            addQuad(vWhite, wIdx, x0, y0, x1, y1);
        }
    }

    whiteKeysNode->setGeometry(whiteGeom);
    whiteKeysNode->markDirty(QSGNode::DirtyGeometry);

    cKeysNode->setGeometry(cGeom);
    cKeysNode->markDirty(QSGNode::DirtyGeometry);

    blackKeysNode->setGeometry(blackGeom);
    blackKeysNode->markDirty(QSGNode::DirtyGeometry);

    // Build divider lines
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
