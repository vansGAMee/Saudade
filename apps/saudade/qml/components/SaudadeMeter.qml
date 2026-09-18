import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: meter

    property real level: 0.0 // 0.0 to 1.0 (linear or dB normalized)
    property real peak: 0.0
    property real meterHeight: 80
    property real meterWidth: 6

    implicitWidth: meterWidth
    implicitHeight: meterHeight

    // Background groove
    Rectangle {
        anchors.fill: parent
        radius: 1
        color: SaudadeTheme.bgCanvas
        border.width: 1
        border.color: SaudadeTheme.lineSoft

        // Active Level Fill (Rises from bottom)
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 2
            height: Math.max(0, (parent.height - 2) * Math.min(1.0, meter.level))
            radius: 1

            // Color gradient/segmentation based on level
            color: {
                if (meter.level > 0.9) return SaudadeTheme.accentRecord;
                if (meter.level > 0.75) return SaudadeTheme.accentWarning;
                return SaudadeTheme.accentSuccess;
            }
        }

        // Peak Hold Line
        Rectangle {
            visible: meter.peak > 0.02
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 2
            height: 1.5
            y: Math.max(1, (parent.height - 2) * (1.0 - Math.min(1.0, meter.peak)))
            color: meter.peak > 0.9 ? SaudadeTheme.accentRecord : SaudadeTheme.textPrimary
        }
    }
}
