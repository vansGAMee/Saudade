import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: transportBar

    property var controller: null
    property bool loopEnabled: true
    property bool recordArmed: false

    signal searchTriggered()
    signal settingsTriggered()

    height: SaudadeTheme.topBarHeight
    color: SaudadeTheme.bgCanvas
    border.width: 0

    // Bottom structural separator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineNormal
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        // Saudade Wordmark
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "S A U D A D E"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 2
            color: SaudadeTheme.textPrimary
        }

        // Structural separator
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 14
            color: SaudadeTheme.lineSoft
        }

        // Project Name
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Vektor_Overdrive.sd"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            color: SaudadeTheme.textSecondary
        }

        // Undo / Redo
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            SaudadeIconButton {
                iconName: "undo"
                variant: "subtle"
                tooltipText: "Undo (Ctrl+Z)"
                width: 24
                height: 24
                iconSize: 12
            }
            SaudadeIconButton {
                iconName: "redo"
                variant: "subtle"
                tooltipText: "Redo (Ctrl+Y)"
                width: 24
                height: 24
                iconSize: 12
            }
        }
    }

    // Center Transport & Master Clock Area
    Row {
        anchors.centerIn: parent
        spacing: 12

        // Transport Controls Cluster
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 32
            width: transportControlsRow.width + 8
            radius: SaudadeTheme.radiusMd
            color: SaudadeTheme.bgWorkspace
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            Row {
                id: transportControlsRow
                anchors.centerIn: parent
                spacing: 3

                // Rewind
                SaudadeIconButton {
                    iconName: "rewind"
                    variant: "subtle"
                    width: 26
                    height: 26
                    iconSize: 12
                    onClicked: {
                        if (transportBar.controller) {
                            transportBar.controller.stop();
                        }
                    }
                }

                // Stop
                SaudadeIconButton {
                    iconName: "stop"
                    variant: (transportBar.controller && !transportBar.controller.isPlaying) ? "secondary" : "subtle"
                    width: 26
                    height: 26
                    iconSize: 12
                    onClicked: {
                        if (transportBar.controller) {
                            transportBar.controller.stop();
                        }
                    }
                }

                // Play
                SaudadeIconButton {
                    iconName: "play"
                    variant: (transportBar.controller && transportBar.controller.isPlaying) ? "primary" : "secondary"
                    width: 32
                    height: 26
                    iconSize: 14
                    onClicked: {
                        if (transportBar.controller) {
                            if (transportBar.controller.isPlaying) {
                                transportBar.controller.stop();
                            } else {
                                transportBar.controller.play();
                            }
                        }
                    }
                }

                // Record (NO STATUS DOT: uses semantic color & border state)
                SaudadeIconButton {
                    iconName: "record"
                    variant: "record"
                    checkable: true
                    checked: transportBar.recordArmed
                    width: 26
                    height: 26
                    iconSize: 12
                    onClicked: {
                        transportBar.recordArmed = !transportBar.recordArmed;
                    }
                }

                // Loop
                SaudadeIconButton {
                    iconName: "loop"
                    variant: transportBar.loopEnabled ? "secondary" : "subtle"
                    checkable: true
                    checked: transportBar.loopEnabled
                    width: 26
                    height: 26
                    iconSize: 12
                    onClicked: {
                        transportBar.loopEnabled = !transportBar.loopEnabled;
                    }
                }
            }
        }

        // Master Clock & Metric Readout Inset Block
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 32
            width: clockRow.width + 16
            radius: SaudadeTheme.radiusMd
            color: SaudadeTheme.bgWorkspace
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            readonly property real curBeat: transportBar.controller ? transportBar.controller.currentBeat : 0.0
            readonly property real bpmVal: transportBar.controller ? transportBar.controller.bpm : 120.0

            function formatSMPTE(beat, bpm) {
                var totalSec = (beat / Math.max(1.0, bpm)) * 60.0;
                var mins = Math.floor(totalSec / 60.0);
                var secs = Math.floor(totalSec % 60.0);
                var ms = Math.floor((totalSec % 1.0) * 1000.0);
                var minStr = mins < 10 ? "0" + mins : "" + mins;
                var secStr = secs < 10 ? "0" + secs : "" + secs;
                var msStr = ms < 10 ? "00" + ms : (ms < 100 ? "0" + ms : "" + ms);
                return "00:" + minStr + ":" + secStr + "." + msStr;
            }

            function formatBarBeat(beat) {
                var bar = Math.floor(beat / 4.0) + 1;
                var bInBar = Math.floor(beat % 4.0) + 1;
                var sub = Math.floor((beat % 1.0) * 4.0) + 1;
                return bar + "." + bInBar + "." + sub;
            }

            Row {
                id: clockRow
                anchors.centerIn: parent
                spacing: 10

                // SMPTE Timecode
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "SMPTE"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: parent.parent.parent.formatSMPTE(parent.parent.parent.curBeat, parent.parent.parent.bpmVal)
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }
                }

                // Vertical hairline
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 16
                    color: SaudadeTheme.lineSoft
                }

                // Bar.Beat Position
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "BAR"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: parent.parent.parent.formatBarBeat(parent.parent.parent.curBeat)
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.accentPlayhead
                    }
                }

                // Vertical hairline
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 16
                    color: SaudadeTheme.lineSoft
                }

                // TEMPO
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "TEMPO"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: parent.parent.parent.bpmVal.toFixed(2)
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }
                }

                // Vertical hairline
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 16
                    color: SaudadeTheme.lineSoft
                }

                // TIME SIGNATURE
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "SIG"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: "4/4"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }
                }
            }
        }
    }

    // Right Engine Status & Global Utilities
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        // PipeWire Endpoint Status Inset
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 30
            width: engineStatusRow.width + 12
            radius: SaudadeTheme.radiusMd
            color: SaudadeTheme.bgWorkspace
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            Row {
                id: engineStatusRow
                anchors.centerIn: parent
                spacing: 8

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "PIPEWIRE 48k"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: "1.3 ms"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 14
                    color: SaudadeTheme.lineSoft
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "DSP LOAD"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: (transportBar.controller && transportBar.controller.isPlaying) ? "1.2%" : "0.3%"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.accentSuccess
                    }
                }
            }
        }

        // Search Pill
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            width: 100
            radius: SaudadeTheme.radiusMd
            color: SaudadeTheme.bgControl
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "Search"
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: SaudadeTheme.textSmall
                    color: SaudadeTheme.textSecondary
                }

                Rectangle {
                    width: 38
                    height: 16
                    radius: 2
                    color: SaudadeTheme.bgWorkspace
                    border.width: 1
                    border.color: SaudadeTheme.lineSoft

                    Text {
                        anchors.centerIn: parent
                        text: "Ctrl+K"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 9
                        color: SaudadeTheme.textMuted
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: transportBar.searchTriggered()
            }
        }

        // Settings / Workspace Action
        SaudadeIconButton {
            iconName: "settings"
            variant: "subtle"
            width: 28
            height: 28
            iconSize: 14
            onClicked: transportBar.settingsTriggered()
        }
    }
}
