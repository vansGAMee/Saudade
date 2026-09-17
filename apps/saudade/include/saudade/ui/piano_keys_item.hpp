#pragma once

#include <saudade/ui/coordinates.hpp>

#include <QQuickItem>

namespace saudade::ui {

/// Piano keys strip rendered along the left side of the Piano Roll.
/// Vertically matches the Piano Roll rows and visualizes white vs black keys.
class PianoKeysItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(float rowHeight READ rowHeight WRITE setRowHeight NOTIFY rowHeightChanged)
    Q_PROPERTY(int minPitch READ minPitch WRITE setMinPitch NOTIFY pitchRangeChanged)
    Q_PROPERTY(int maxPitch READ maxPitch WRITE setMaxPitch NOTIFY pitchRangeChanged)

public:
    explicit PianoKeysItem(QQuickItem* parent = nullptr);
    ~PianoKeysItem() override = default;

    [[nodiscard]] float rowHeight() const noexcept { return row_height_; }
    void setRowHeight(float h);

    [[nodiscard]] int minPitch() const noexcept { return min_pitch_; }
    void setMinPitch(int p);

    [[nodiscard]] int maxPitch() const noexcept { return max_pitch_; }
    void setMaxPitch(int p);

signals:
    void rowHeightChanged();
    void pitchRangeChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    float row_height_{Coordinates::kDefaultRowHeight};
    int min_pitch_{Coordinates::kDefaultMinPitch};
    int max_pitch_{Coordinates::kDefaultMaxPitch};
};

} // namespace saudade::ui
