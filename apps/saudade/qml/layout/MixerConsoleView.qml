import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: mixerView

    property var controller: null

    color: SaudadeTheme.bgWorkspace

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: channelsRow.implicitWidth + 24
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Row {
            id: channelsRow
            spacing: 8
            height: parent.height - 24

            // Channel 1: Kick
            ChannelStrip {
                channelNumber: "01"
                channelName: "Kick & Sub"
                faderVal: 0.82
                meterVal: (mixerView.controller && mixerView.controller.isPlaying) ? 0.72 : 0.0
            }

            // Channel 2: PolySynth (Linked to synth!)
            ChannelStrip {
                channelNumber: "02"
                channelName: "PolySynth"
                isMidi: true
                selected: true
                faderVal: 0.85
                meterVal: (mixerView.controller && mixerView.controller.isPlaying) ? 0.78 : 0.0
            }

            // Channel 3: Atmosphere
            ChannelStrip {
                channelNumber: "03"
                channelName: "Atmosphere"
                faderVal: 0.65
                meterVal: (mixerView.controller && mixerView.controller.isPlaying) ? 0.45 : 0.0
            }

            // Channel 4: Glitch Perc
            ChannelStrip {
                channelNumber: "04"
                channelName: "Glitch Perc"
                faderVal: 0.70
                meterVal: (mixerView.controller && mixerView.controller.isPlaying) ? 0.55 : 0.0
            }

            // Channel 5: Lead Vocal
            ChannelStrip {
                channelNumber: "05"
                channelName: "Lead Vocal"
                faderVal: 0.75
                meterVal: (mixerView.controller && mixerView.controller.isPlaying) ? 0.60 : 0.0
            }

            // Master Bus Channel Strip
            Rectangle {
                width: 100
                height: parent.height
                radius: SaudadeTheme.radiusMd
                color: SaudadeTheme.bgPanelRaised
                border.width: 1.5
                border.color: SaudadeTheme.lineFocus

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        text: "MASTER"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }

                    Rectangle { width: parent.width; height: 1; color: SaudadeTheme.lineSoft }

                    SaudadeKnob {
                        label: "PAN"
                        value: 0.5
                        displayText: "C"
                        knobSize: 32
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8
                        height: parent.height - 130

                        SaudadeMeter {
                            meterHeight: parent.height
                            level: (mixerView.controller && mixerView.controller.isPlaying) ? 0.82 : 0.0
                            peak: (mixerView.controller && mixerView.controller.isPlaying) ? 0.86 : 0.0
                        }

                        SaudadeSlider {
                            orientation: "vertical"
                            trackLength: parent.height
                            value: 0.85
                        }

                        SaudadeMeter {
                            meterHeight: parent.height
                            level: (mixerView.controller && mixerView.controller.isPlaying) ? 0.82 : 0.0
                            peak: (mixerView.controller && mixerView.controller.isPlaying) ? 0.86 : 0.0
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "0.0 dB"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }
                }
            }
        }
    }

    component ChannelStrip: Rectangle {
        property string channelNumber: "01"
        property string channelName: "Channel"
        property bool isMidi: false
        property bool selected: false
        property real faderVal: 0.75
        property real meterVal: 0.0

        width: 88
        height: parent.height
        radius: SaudadeTheme.radiusMd
        color: selected ? SaudadeTheme.bgPanelRaised : SaudadeTheme.bgPanel
        border.width: 1
        border.color: selected ? SaudadeTheme.lineFocus : SaudadeTheme.lineNormal

        Column {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6

            // Header: Number + Name
            Row {
                width: parent.width
                spacing: 4
                Text {
                    text: channelNumber
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 9
                    color: selected ? SaudadeTheme.accentSelection : SaudadeTheme.textMuted
                }
                Text {
                    text: channelName
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 10
                    font.weight: selected ? Font.DemiBold : Font.Medium
                    color: selected ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary
                    elide: Text.ElideRight
                    width: 58
                }
            }

            Rectangle { width: parent.width; height: 1; color: SaudadeTheme.lineSoft }

            // Pan Knob
            SaudadeKnob {
                label: "PAN"
                value: 0.5
                displayText: "C"
                knobSize: 28
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // Meter + Fader Stage
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                height: parent.height - 146

                SaudadeMeter {
                    meterHeight: parent.height
                    level: meterVal
                    peak: meterVal > 0 ? meterVal + 0.04 : 0.0
                }

                SaudadeSlider {
                    orientation: "vertical"
                    trackLength: parent.height
                    value: faderVal
                }
            }

            // dB Readout
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "-3.2 dB"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 9
                color: SaudadeTheme.textSecondary
            }

            // Mute / Solo / Arm Row
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3

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
            }
        }
    }
}
