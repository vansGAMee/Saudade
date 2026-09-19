#pragma once

#include <QQuickItem>
#include <QColor>

#include <saudade/ui/editor_controller.hpp>
#include <saudade/ui/coordinates.hpp>

namespace saudade::ui {

/// High-performance custom Scene Graph Piano Roll view and interactive editor.
/// Renders pitch/beat grid, notes, and playhead via QSG nodes.
class PianoRollItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(saudade::ui::EditorController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(float beatWidth READ beatWidth WRITE setBeatWidth NOTIFY beatWidthChanged)
    Q_PROPERTY(float rowHeight READ rowHeight WRITE setRowHeight NOTIFY rowHeightChanged)
    Q_PROPERTY(int minPitch READ minPitch WRITE setMinPitch NOTIFY pitchRangeChanged)
    Q_PROPERTY(int maxPitch READ maxPitch WRITE setMaxPitch NOTIFY pitchRangeChanged)

public:
    explicit PianoRollItem(QQuickItem* parent = nullptr);
    ~PianoRollItem() override = default;

    [[nodiscard]] EditorController* controller() const noexcept { return controller_; }
    void setController(EditorController* controller);

    [[nodiscard]] const QString& activeTool() const noexcept { return active_tool_; }
    void setActiveTool(const QString& tool);

    [[nodiscard]] float beatWidth() const noexcept { return beat_width_; }
    void setBeatWidth(float w);

    [[nodiscard]] float rowHeight() const noexcept { return row_height_; }
    void setRowHeight(float h);

    [[nodiscard]] int minPitch() const noexcept { return min_pitch_; }
    void setMinPitch(int p);

    [[nodiscard]] int maxPitch() const noexcept { return max_pitch_; }
    void setMaxPitch(int p);

signals:
    void controllerChanged();
    void activeToolChanged();
    void beatWidthChanged();
    void rowHeightChanged();
    void pitchRangeChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    enum class DragMode {
        None,
        Create,
        Move,
        ResizeStart,
        ResizeEnd,
        Erase,
        Marquee
    };

    struct HitResult {
        bool hit{false};
        bool is_start_resize_handle{false};
        bool is_end_resize_handle{false};
        uint64_t note_id{0};
        double pitch{0.0};
        time::BeatPosition start{0};
        time::BeatDuration duration{0};
    };

    struct InitialNoteState {
        uint64_t note_id{0};
        time::BeatPosition start{0};
        double pitch{0.0};
        time::BeatDuration duration{0};
    };

    [[nodiscard]] HitResult hitTest(float x, float y) const noexcept;

    EditorController* controller_{nullptr};
    QString active_tool_{"pencil"};
    float beat_width_{Coordinates::kDefaultBeatWidth};
    float row_height_{Coordinates::kDefaultRowHeight};
    int min_pitch_{Coordinates::kDefaultMinPitch};
    int max_pitch_{Coordinates::kDefaultMaxPitch};

    DragMode drag_mode_{DragMode::None};
    uint64_t drag_note_id_{0};
    QPointF mouse_press_pos_{};
    time::BeatPosition initial_note_start_{0};
    double initial_note_pitch_{60.0};
    time::BeatDuration initial_note_duration_{0};

    std::vector<InitialNoteState> initial_selected_states_{};
    QRectF marquee_rect_{};
    bool is_marquee_active_{false};
    double last_audition_pitch_{-1.0};
    bool temporary_erase_{false};
};

} // namespace saudade::ui
