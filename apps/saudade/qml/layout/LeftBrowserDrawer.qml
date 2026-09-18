import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: drawer

    property bool collapsed: false
    property string activeCategory: "Samples" // "Samples", "Plugins", "Presets", "Projects"

    signal closeRequested()

    width: collapsed ? 0 : SaudadeTheme.drawerWidth
    visible: width > 0
    clip: true
    color: SaudadeTheme.bgPanel
    border.width: 0

    // Right border separator
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: SaudadeTheme.lineNormal
    }

    Column {
        anchors.fill: parent
        anchors.rightMargin: 1
        spacing: 0

        // Drawer Header
        Rectangle {
            width: parent.width
            height: 36
            color: SaudadeTheme.bgWorkspace
            border.width: 0

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: SaudadeTheme.lineSoft
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    text: "ASSET MATRIX"
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                    color: SaudadeTheme.textPrimary
                }

                Rectangle {
                    width: 16
                    height: 14
                    radius: 2
                    color: SaudadeTheme.bgControl
                    Text {
                        anchors.centerIn: parent
                        text: "B"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        color: SaudadeTheme.textMuted
                    }
                }
            }

            SaudadeIconButton {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                iconName: "close"
                variant: "subtle"
                width: 22
                height: 22
                iconSize: 11
                onClicked: drawer.closeRequested()
            }
        }

        // Search Input Filter
        Rectangle {
            width: parent.width
            height: 34
            color: SaudadeTheme.bgPanel
            border.width: 0

            Rectangle {
                anchors.centerIn: parent
                width: parent.width - 16
                height: 24
                radius: SaudadeTheme.radiusSm
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: searchInput.activeFocus ? SaudadeTheme.lineFocus : SaudadeTheme.lineSoft

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "⌕"
                        font.pixelSize: 12
                        color: SaudadeTheme.textMuted
                    }

                    TextInput {
                        id: searchInput
                        width: parent.width - 24
                        font.family: SaudadeTheme.fontSans
                        font.pixelSize: SaudadeTheme.textSmall
                        color: SaudadeTheme.textPrimary
                        selectByMouse: true
                        clip: true

                        Text {
                            anchors.fill: parent
                            visible: !searchInput.text && !searchInput.activeFocus
                            text: "Filter library..."
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: SaudadeTheme.textSmall
                            color: SaudadeTheme.textGhost
                        }
                    }
                }
            }
        }

        // Category Filter Tabs
        Rectangle {
            width: parent.width
            height: 28
            color: SaudadeTheme.bgPanel
            border.width: 0

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Repeater {
                    model: ["Samples", "Plugins", "Presets", "Projects"]

                    Rectangle {
                        width: catText.implicitWidth + 10
                        height: 20
                        radius: SaudadeTheme.radiusSm
                        color: drawer.activeCategory === modelData
                               ? SaudadeTheme.bgPanelRaised
                               : (catMouse.containsMouse ? SaudadeTheme.bgControlHover : "transparent")
                        border.width: 1
                        border.color: drawer.activeCategory === modelData ? SaudadeTheme.lineNormal : "transparent"

                        Text {
                            id: catText
                            anchors.centerIn: parent
                            text: modelData
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: 9
                            font.weight: drawer.activeCategory === modelData ? Font.Medium : Font.Normal
                            color: drawer.activeCategory === modelData ? SaudadeTheme.textPrimary : SaudadeTheme.textMuted
                        }

                        MouseArea {
                            id: catMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: drawer.activeCategory = modelData
                        }
                    }
                }
            }
        }

        // Tree List View
        Flickable {
            width: parent.width
            height: parent.height - 36 - 34 - 28 - 24
            contentWidth: width
            contentHeight: treeContentCol.implicitHeight + 16
            clip: true

            Column {
                id: treeContentCol
                width: parent.width
                spacing: 6
                topPadding: 6

                // Section 1: Drum Hits
                Column {
                    width: parent.width
                    spacing: 2

                    // Category Header
                    Row {
                        leftPadding: 8
                        spacing: 4
                        Text { text: "▾"; font.pixelSize: 9; color: SaudadeTheme.textSecondary }
                        Text {
                            text: "DRUM HITS (48kHz 24b)"
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            color: SaudadeTheme.textSecondary
                        }
                    }

                    // Items
                    Repeater {
                        model: [
                            { name: "Kick_808_Sub.wav", dur: "0.4s", type: "WAV" },
                            { name: "Snare_Machined_01.wav", dur: "0.2s", type: "WAV" },
                            { name: "HiHat_Tight_02.wav", dur: "0.1s", type: "WAV" },
                            { name: "Clap_Stereo_Layer.wav", dur: "0.3s", type: "WAV" }
                        ]

                        Rectangle {
                            width: drawer.width - 16
                            x: 8
                            height: 22
                            radius: 2
                            color: itemMouse.containsMouse ? SaudadeTheme.bgControlHover : "transparent"

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Text {
                                    text: "♪"
                                    font.pixelSize: 10
                                    color: SaudadeTheme.textMuted
                                }
                                Text {
                                    text: modelData.name
                                    font.family: SaudadeTheme.fontSans
                                    font.pixelSize: SaudadeTheme.textSmall
                                    color: itemMouse.containsMouse ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary
                                    elide: Text.ElideRight
                                    width: 140
                                }
                                Item { width: 10; height: 1 }
                                Text {
                                    text: modelData.dur
                                    font.family: SaudadeTheme.fontMono
                                    font.pixelSize: 8
                                    color: SaudadeTheme.textMuted
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }

                // Section 2: Synths & Presets
                Column {
                    width: parent.width
                    spacing: 2

                    Row {
                        leftPadding: 8
                        spacing: 4
                        Text { text: "▾"; font.pixelSize: 9; color: SaudadeTheme.textSecondary }
                        Text {
                            text: "SYNTH PRESETS"
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            color: SaudadeTheme.textSecondary
                        }
                    }

                    Repeater {
                        model: [
                            { name: "PolySynth_DarkLead.sdpreset", type: "SYNTH" },
                            { name: "Ambient_Pad_01.sdpreset", type: "SYNTH" },
                            { name: "SubBass_Mono.sdpreset", type: "SYNTH" }
                        ]

                        Rectangle {
                            width: drawer.width - 16
                            x: 8
                            height: 22
                            radius: 2
                            color: presetMouse.containsMouse ? SaudadeTheme.bgControlHover : "transparent"

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Text {
                                    text: "◈"
                                    font.pixelSize: 9
                                    color: SaudadeTheme.accentSelection
                                }
                                Text {
                                    text: modelData.name
                                    font.family: SaudadeTheme.fontSans
                                    font.pixelSize: SaudadeTheme.textSmall
                                    color: presetMouse.containsMouse ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary
                                    elide: Text.ElideRight
                                    width: 180
                                }
                            }

                            MouseArea {
                                id: presetMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }
            }
        }

        // Bottom status chip
        Rectangle {
            width: parent.width
            height: 24
            color: SaudadeTheme.bgWorkspace
            border.width: 0

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: SaudadeTheme.lineSoft
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: "LIBRARY: 128 ITEMS"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    color: SaudadeTheme.textMuted
                }
                Text {
                    text: "•"
                    font.pixelSize: 8
                    color: SaudadeTheme.lineSoft
                }
                Text {
                    text: "48 kHz"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    color: SaudadeTheme.textMuted
                }
            }
        }
    }

    Behavior on width {
        NumberAnimation { duration: 140; easing.type: Easing.OutQuad }
    }
}
