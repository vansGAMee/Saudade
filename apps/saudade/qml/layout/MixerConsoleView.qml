import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Item {
    id: mixerView
    property var controller: null

    Rectangle { anchors.fill: parent; color: SaudadeTheme.bgWorkspace }

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: strips.width
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        Row {
            id: strips
            height: parent.height
            spacing: 8

            Repeater {
                model: mixerView.controller ? mixerView.controller.tracksData : []
                delegate: Rectangle {
                    required property var modelData
                    width: 104
                    height: strips.height
                    radius: SaudadeTheme.radiusMd
                    color: modelData.selected ? SaudadeTheme.bgPanelRaised
                                              : SaudadeTheme.bgPanel
                    border.width: 1
                    border.color: modelData.selected ? SaudadeTheme.lineFocus
                                                     : SaudadeTheme.lineNormal

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text {
                            width: parent.width
                            text: modelData.name
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: SaudadeTheme.textPrimary
                        }

                        SaudadeKnob {
                            anchors.horizontalCenter: parent.horizontalCenter
                            label: "Pan"
                            knobSize: 34
                            value: (modelData.pan + 1.0) * 0.5
                            defaultValue: 0.5
                            displayText: Math.abs(modelData.pan) < 0.01
                                         ? "C"
                                         : (modelData.pan < 0
                                            ? Math.round(-modelData.pan * 100) + "L"
                                            : Math.round(modelData.pan * 100) + "R")
                            onValueModified: function(newValue) {
                                mixerView.controller.setTrackPan(
                                            modelData.id, newValue * 2.0 - 1.0)
                            }
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            height: Math.max(80, parent.height - 190)
                            spacing: 8

                            SaudadeSlider {
                                orientation: "vertical"
                                trackLength: parent.height
                                value: (Math.max(-60, Math.min(12, modelData.gainDb)) + 60) / 72
                                onValueModified: function(newValue) {
                                    mixerView.controller.setTrackGain(
                                                modelData.id, newValue * 72 - 60)
                                }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Number(modelData.gainDb).toFixed(1) + " dB"
                            font.family: SaudadeTheme.fontMono
                            font.pixelSize: 9
                            color: SaudadeTheme.textSecondary
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 5
                            SaudadeButton {
                                text: "M"
                                compact: true
                                checkable: true
                                checked: modelData.muted
                                tooltipText: "Mute track"
                                onClicked: mixerView.controller.setTrackMute(
                                               modelData.id, checked)
                            }
                            SaudadeButton {
                                text: "S"
                                compact: true
                                checkable: true
                                checked: modelData.solo
                                tooltipText: "Solo track"
                                onClicked: mixerView.controller.setTrackSolo(
                                               modelData.id, checked)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        onClicked: mixerView.controller.selectTrack(modelData.id)
                    }
                }
            }

            Rectangle {
                width: 132
                height: strips.height
                radius: SaudadeTheme.radiusMd
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: SaudadeTheme.lineFocus

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "MASTER"
                        font.family: SaudadeTheme.fontSans
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: Math.max(100, parent.height - 100)
                        spacing: 12
                        SaudadeMeter {
                            meterHeight: parent.height
                            level: mixerView.controller ? mixerView.controller.meterLeft : 0
                            peak: level
                        }
                        SaudadeSlider {
                            orientation: "vertical"
                            trackLength: parent.height
                            value: mixerView.controller
                                   ? (mixerView.controller.masterGainDb + 60) / 72 : 0.833
                            onValueModified: function(newValue) {
                                mixerView.controller.setMasterGainDb(newValue * 72 - 60)
                            }
                        }
                        SaudadeMeter {
                            meterHeight: parent.height
                            level: mixerView.controller ? mixerView.controller.meterRight : 0
                            peak: level
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: mixerView.controller
                              ? Number(mixerView.controller.masterGainDb).toFixed(1) + " dB"
                              : "0.0 dB"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 10
                        color: SaudadeTheme.textPrimary
                    }
                }
            }
        }
    }
}
