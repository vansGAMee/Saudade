import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: slider

    property real value: 0.75 // 0.0 to 1.0
    property string orientation: "vertical" // "vertical" or "horizontal"
    property real trackLength: 100
    property real trackThickness: 4
    property real thumbWidth: 20
    property real thumbHeight: 10

    signal valueModified(real newValue)

    implicitWidth: orientation === "vertical" ? thumbWidth : trackLength
    implicitHeight: orientation === "vertical" ? trackLength : thumbWidth

    readonly property bool isHovered: mouseArea.containsMouse
    readonly property bool isPressed: mouseArea.pressed

    // Inset Track Groove
    Rectangle {
        id: track
        anchors.centerIn: parent
        width: slider.orientation === "vertical" ? slider.trackThickness : slider.trackLength
        height: slider.orientation === "vertical" ? slider.trackLength : slider.trackThickness
        radius: 2
        color: SaudadeTheme.bgCanvas
        border.width: 1
        border.color: SaudadeTheme.lineSoft
    }

    // Machined rectangular thumb cap
    Rectangle {
        id: thumb
        width: slider.orientation === "vertical" ? slider.thumbWidth : slider.thumbHeight
        height: slider.orientation === "vertical" ? slider.thumbHeight : slider.thumbWidth
        radius: 2
        color: slider.isPressed ? SaudadeTheme.bgControlPressed : (slider.isHovered ? SaudadeTheme.bgControlHover : SaudadeTheme.bgControl)
        border.width: 1
        border.color: slider.isHovered ? SaudadeTheme.lineFocus : SaudadeTheme.lineNormal

        x: slider.orientation === "vertical"
           ? (parent.width - width) * 0.5
           : (slider.value * (slider.trackLength - width))

        y: slider.orientation === "vertical"
           ? ((1.0 - slider.value) * (slider.trackLength - height))
           : (parent.height - height) * 0.5

        // Center machined line
        Rectangle {
            anchors.centerIn: parent
            width: slider.orientation === "vertical" ? parent.width - 4 : 2
            height: slider.orientation === "vertical" ? 1.5 : parent.height - 4
            color: slider.isHovered ? SaudadeTheme.textPrimary : SaudadeTheme.textMuted
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: slider.orientation === "vertical" ? Qt.SizeVerCursor : Qt.SizeHorCursor

        function updateFromPos(mouseX, mouseY) {
            if (slider.orientation === "vertical") {
                var availH = slider.trackLength - thumb.height;
                var norm = 1.0 - (mouseY - thumb.height * 0.5) / availH;
                var clamped = Math.max(0.0, Math.min(1.0, norm));
                slider.value = clamped;
                slider.valueModified(clamped);
            } else {
                var availW = slider.trackLength - thumb.width;
                var normW = (mouseX - thumb.width * 0.5) / availW;
                var clampedW = Math.max(0.0, Math.min(1.0, normW));
                slider.value = clampedW;
                slider.valueModified(clampedW);
            }
        }

        onPressed: (mouse) => updateFromPos(mouse.x, mouse.y)
        onPositionChanged: (mouse) => {
            if (pressed) updateFromPos(mouse.x, mouse.y);
        }
    }
}
