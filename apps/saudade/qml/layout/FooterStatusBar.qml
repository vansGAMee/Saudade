import QtQuick
import QtQuick.Controls
import "../theme"

Rectangle {
    id: footer

    property var controller: null

    height: SaudadeTheme.footerHeight
    color: SaudadeTheme.bgCanvas
    border.width: 0

    // Top border separator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineNormal
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Saudade Engine v2.4.1-rt"
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            color: SaudadeTheme.textMuted
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "•"
            font.pixelSize: 9
            color: SaudadeTheme.textGhost
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "ALSA/PipeWire Realtime (Priority 95)"
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            color: SaudadeTheme.textSecondary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "•"
            font.pixelSize: 9
            color: SaudadeTheme.textGhost
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Buffer Underruns: 0"
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            color: SaudadeTheme.accentSuccess
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "C++23 • Zero-Allocation RT Core • Linux-First Architecture"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: 9
            color: SaudadeTheme.textMuted
        }
    }
}
