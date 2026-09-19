import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: drawer

    property bool collapsed: true
    signal closeRequested()
    signal openProjectRequested()
    signal importMidiRequested()
    signal exportWavRequested()

    width: collapsed ? 0 : 268
    color: SaudadeTheme.bgPanel
    clip: true
    visible: width > 0

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: SaudadeTheme.lineNormal
    }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Row {
            width: parent.width
            Text {
                width: parent.width - 28
                text: "PROJECT"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: SaudadeTheme.textPrimary
            }
            SaudadeButton {
                text: "×"
                compact: true
                tooltipText: "Close browser"
                onClicked: drawer.closeRequested()
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Bring musical material into the current arrangement or open another Saudade project."
            font.family: SaudadeTheme.fontSans
            font.pixelSize: 11
            lineHeight: 1.25
            color: SaudadeTheme.textSecondary
        }

        SaudadeButton {
            width: parent.width
            text: "Open Project"
            tooltipText: "Open project (Ctrl+O)"
            onClicked: drawer.openProjectRequested()
        }
        SaudadeButton {
            width: parent.width
            text: "Import MIDI"
            variant: "primary"
            tooltipText: "Import MIDI (Ctrl+I)"
            onClicked: drawer.importMidiRequested()
        }
        SaudadeButton {
            width: parent.width
            text: "Export WAV"
            tooltipText: "Render active loop or project (Ctrl+E)"
            onClicked: drawer.exportWavRequested()
        }

        Rectangle {
            width: parent.width
            height: 1
            color: SaudadeTheme.lineSoft
        }

        Text {
            text: "SUPPORTED NOW"
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            color: SaudadeTheme.textMuted
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Standard MIDI Files\nSaudade .dawproj projects\nStereo PCM WAV export"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: 11
            lineHeight: 1.35
            color: SaudadeTheme.textSecondary
        }
    }

    Behavior on width {
        NumberAnimation { duration: 140; easing.type: Easing.OutQuad }
    }
}
