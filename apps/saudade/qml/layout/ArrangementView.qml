import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Item {
    id: arrangeView

    property var controller: null
    property real beatWidth: 100.0
    property real trackHeaderWidth: 210.0
    property real trackHeight: 64.0

    signal openPianoRollRequested()

    Rectangle {
        anchors.fill: parent
        color: SaudadeTheme.bgWorkspace

        Column {
            anchors.fill: parent
            spacing: 0

            // ==========================================
            // Timeline Ruler
            // ==========================================
            Rectangle {
                width: parent.width
                height: 26
                color: SaudadeTheme.bgCanvas
                border.width: 0

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: SaudadeTheme.lineNormal
                }

                Row {
                    anchors.fill: parent

                    // Ruler track header spacer
                    Rectangle {
                        width: arrangeView.trackHeaderWidth
                        height: parent.height
                        color: SaudadeTheme.bgCanvas
                        border.width: 0

                        Rectangle {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            width: 1
                            color: SaudadeTheme.lineNormal
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Text {
                                text: "TRACKS"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                color: SaudadeTheme.textMuted
                            }
                            Text {
                                text: "6 ACTIVE"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 8
                                color: SaudadeTheme.textGhost
                            }
                        }
                    }

                    // Ruler Bar Numbers
                    Item {
                        width: parent.width - arrangeView.trackHeaderWidth
                        height: parent.height
                        clip: true

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0

                            Repeater {
                                model: 16
                                Item {
                                    width: arrangeView.beatWidth * 4 // 1 Bar = 4 beats
                                    height: 26

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: (index + 1) + ".1"
                                        font.family: SaudadeTheme.fontMono
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: SaudadeTheme.textSecondary
                                    }

                                    // Bar tick
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        height: 8
                                        color: SaudadeTheme.lineNormal
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ==========================================
            // Timeline Body: Tracks Column + Arranger Canvas
            // ==========================================
            Flickable {
                width: parent.width
                height: parent.height - 26
                contentWidth: width
                contentHeight: tracksColumn.implicitHeight + 40
                clip: true

                Row {
                    width: parent.width

                    // 1. Left Track Headers Column
                    Column {
                        id: tracksColumn
                        width: arrangeView.trackHeaderWidth
                        spacing: 1

                        // Track 1: Kick & Sub (Audio)
                        TrackHeader {
                            trackNumber: "01"
                            trackName: "Kick & Sub"
                            trackType: "AUDIO"
                            trackHeight: arrangeView.trackHeight
                        }

                        // Track 2: Analog Poly Synth (MIDI - Selected)
                        TrackHeader {
                            trackNumber: "02"
                            trackName: "Analog Poly Synth"
                            trackType: "MIDI"
                            trackHeight: arrangeView.trackHeight
                            selected: true
                            onDoubleClicked: arrangeView.openPianoRollRequested()
                        }

                        // Track 3: Granular Atmosphere (Audio)
                        TrackHeader {
                            trackNumber: "03"
                            trackName: "Atmosphere"
                            trackType: "AUDIO"
                            trackHeight: arrangeView.trackHeight
                        }

                        // Track 4: Glitch Perc (Audio)
                        TrackHeader {
                            trackNumber: "04"
                            trackName: "Glitch Perc"
                            trackType: "AUDIO"
                            trackHeight: arrangeView.trackHeight
                        }

                        // Track 5: Lead Vocal Comp (Audio)
                        TrackHeader {
                            trackNumber: "05"
                            trackName: "Lead Vocal"
                            trackType: "AUDIO"
                            trackHeight: arrangeView.trackHeight
                        }
                    }

                    // 2. Right Timeline Grid & Clips
                    Item {
                        width: parent.width - arrangeView.trackHeaderWidth
                        height: tracksColumn.implicitHeight
                        clip: true

                        // Grid lines
                        Row {
                            anchors.fill: parent
                            Repeater {
                                model: 16
                                Rectangle {
                                    width: arrangeView.beatWidth * 4
                                    height: parent.height
                                    color: "transparent"
                                    border.width: 1
                                    border.color: SaudadeTheme.lineSoft
                                    opacity: 0.5
                                }
                            }
                        }

                        // Horizontal Track Row Dividers
                        Column {
                            anchors.fill: parent
                            spacing: arrangeView.trackHeight
                            Repeater {
                                model: 6
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: SaudadeTheme.lineSoft
                                }
                            }
                        }

                        // Clips Area
                        Item {
                            anchors.fill: parent

                            // Track 1 Clip: Audio Kick loop
                            Rectangle {
                                x: 0
                                y: 4
                                width: arrangeView.beatWidth * 8
                                height: arrangeView.trackHeight - 8
                                radius: 3
                                color: SaudadeTheme.bgPanelRaised
                                border.width: 1
                                border.color: SaudadeTheme.lineNormal

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.top: parent.top
                                    anchors.topMargin: 4
                                    text: "Kick_808_Sub_Loop.wav"
                                    font.family: SaudadeTheme.fontSans
                                    font.pixelSize: 10
                                    color: SaudadeTheme.textSecondary
                                }
                            }

                            // Track 2 Clip: MIDI Poly Synth (Double-click to open in Piano Roll!)
                            Rectangle {
                                x: 0
                                y: arrangeView.trackHeight + 4
                                width: (arrangeView.controller ? arrangeView.controller.patternLength : 4.0) * arrangeView.beatWidth
                                height: arrangeView.trackHeight - 8
                                radius: 3
                                color: SaudadeTheme.bgPanelRaised
                                border.width: 1.5
                                border.color: SaudadeTheme.accentSelection

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.top: parent.top
                                    anchors.topMargin: 4
                                    text: "Pattern 01 [PolySynth Lead] (Double-click to edit)"
                                    font.family: SaudadeTheme.fontSans
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: SaudadeTheme.textPrimary
                                }

                                // Note preview representations inside clip
                                Row {
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 8
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    spacing: 8

                                    Repeater {
                                        model: 8
                                        Rectangle {
                                            width: 24
                                            height: 6
                                            radius: 1
                                            color: SaudadeTheme.accentSelection
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onDoubleClicked: arrangeView.openPianoRollRequested()
                                }
                            }

                            // Track 3 Clip: Atmosphere Audio
                            Rectangle {
                                x: arrangeView.beatWidth * 4
                                y: arrangeView.trackHeight * 2 + 4
                                width: arrangeView.beatWidth * 12
                                height: arrangeView.trackHeight - 8
                                radius: 3
                                color: SaudadeTheme.bgPanelRaised
                                border.width: 1
                                border.color: SaudadeTheme.lineNormal

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.top: parent.top
                                    anchors.topMargin: 4
                                    text: "Granular_Pad_Texture.wav"
                                    font.family: SaudadeTheme.fontSans
                                    font.pixelSize: 10
                                    color: SaudadeTheme.textMuted
                                }
                            }

                            // Moving Playhead Line
                            Rectangle {
                                id: playheadLine
                                property real curBeat: arrangeView.controller ? arrangeView.controller.currentBeat : 0.0
                                x: curBeat * arrangeView.beatWidth
                                y: 0
                                width: 2
                                height: parent.height
                                color: SaudadeTheme.accentPlayhead
                                z: 10

                                // Playhead Top Flag
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 8
                                    height: 8
                                    rotation: 45
                                    color: SaudadeTheme.accentPlayhead
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Component for Track Header
    component TrackHeader: Rectangle {
        property string trackNumber: "01"
        property string trackName: "Track"
        property string trackType: "AUDIO"
        property real trackHeight: 64
        property bool selected: false

        signal doubleClicked()

        width: arrangeView.trackHeaderWidth
        height: trackHeight
        color: selected ? SaudadeTheme.bgPanelRaised : SaudadeTheme.bgPanel
        border.width: 1
        border.color: selected ? SaudadeTheme.lineFocus : SaudadeTheme.lineNormal

        Column {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            // Top Row: Number, Name, Type
            Row {
                width: parent.width
                spacing: 6

                Text {
                    text: trackNumber
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 9
                    color: selected ? SaudadeTheme.accentSelection : SaudadeTheme.textMuted
                }

                Text {
                    text: trackName
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 11
                    font.weight: selected ? Font.DemiBold : Font.Medium
                    color: selected ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary
                    elide: Text.ElideRight
                    width: 110
                }

                Item { width: 4; height: 1 }

                Text {
                    text: trackType
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    color: SaudadeTheme.textGhost
                }
            }

            // Bottom Row: Mute, Solo, Arm, Vol Fader
            Row {
                width: parent.width
                spacing: 4

                SaudadeButton {
                    text: "M"
                    variant: "secondary"
                    checkable: true
                    compact: true
                    implicitWidth: 20
                    implicitHeight: 18
                }
                SaudadeButton {
                    text: "S"
                    variant: "secondary"
                    checkable: true
                    compact: true
                    implicitWidth: 20
                    implicitHeight: 18
                }
                SaudadeButton {
                    text: "R"
                    variant: "record"
                    checkable: true
                    compact: true
                    implicitWidth: 20
                    implicitHeight: 18
                }

                Item { width: 4; height: 1 }

                // Small Track Volume Slider
                SaudadeSlider {
                    orientation: "horizontal"
                    trackLength: 80
                    value: 0.8
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onDoubleClicked: parent.doubleClicked()
        }
    }
}
