#pragma once

#include <QQuickItem>

#include <saudade/ui/coordinates.hpp>

namespace saudade::ui {

class EditorController;

/// Piano keys strip rendered along the left side of the Piano Roll.
/// Vertically matches the Piano Roll rows and visualizes white vs black keys.
class PianoKeysItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(saudade::ui::EditorController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(float rowHeight READ rowHeight WRITE setRowHeight NOTIFY rowHeightChanged)
    Q_PROPERTY(int minPitch READ minPitch WRITE setMinPitch NOTIFY pitchRangeChanged)
    Q_PROPERTY(int maxPitch READ maxPitch WRITE setMaxPitch NOTIFY pitchRangeChanged)

public:
    explicit PianoKeysItem(QQuickItem* parent = nullptr);
    ~PianoKeysItem() override = default;

    [[nodiscard]] EditorController* controller() const noexcept { return controller_; }
    void setController(EditorController* controller);

    [[nodiscard]] float rowHeight() const noexcept { return row_height_; }
    void setRowHeight(float h);

    [[nodiscard]] int minPitch() const noexcept { return min_pitch_; }
    void setMinPitch(int p);

    [[nodiscard]] int maxPitch() const noexcept { return max_pitch_; }
    void setMaxPitch(int p);

signals:
    void controllerChanged();
    void rowHeightChanged();
    void pitchRangeChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    EditorController* controller_{nullptr};
    float row_height_{Coordinates::kDefaultRowHeight};
    int min_pitch_{Coordinates::kDefaultMinPitch};
    int max_pitch_{Coordinates::kDefaultMaxPitch};
    int pressed_pitch_{-1};
};

} // namespace saudade::ui
