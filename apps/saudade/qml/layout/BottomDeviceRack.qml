import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: deviceRack

    property var controller: null
    property bool collapsed: true

    height: collapsed ? 28 : SaudadeTheme.deviceRackHeight
    color: SaudadeTheme.bgPanel
    border.width: 0

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineNormal
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28
        color: SaudadeTheme.bgPanelRaised

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Text {
                text: deviceRack.collapsed ? "▸" : "▾"
                font.pixelSize: 10
                color: SaudadeTheme.textSecondary
            }
            Text {
                text: "SAUDADE POLYSYNTH"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: SaudadeTheme.textSmall
                font.weight: Font.DemiBold
                color: SaudadeTheme.textPrimary
            }
            Text {
                text: "Warm Lead"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: SaudadeTheme.textSmall
                color: SaudadeTheme.textMuted
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: deviceRack.collapsed = !deviceRack.collapsed
        }
    }

    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !deviceRack.collapsed
        contentWidth: controlsRow.width + 24
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        Row {
            id: controlsRow
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 18

            SaudadeKnob {
                label: "Attack"
                value: deviceRack.controller
                       ? Math.sqrt(deviceRack.controller.synthAttack / 2.0) : 0
                displayText: deviceRack.controller
                             ? Math.round(deviceRack.controller.synthAttack * 1000) + " ms" : ""
                onValueModified: function(v) {
                    deviceRack.controller.setSynthAttack(Math.max(0.001, v * v * 2.0))
                }
            }
            SaudadeKnob {
                label: "Decay"
                value: deviceRack.controller
                       ? Math.sqrt(deviceRack.controller.synthDecay / 4.0) : 0
                displayText: deviceRack.controller
                             ? Number(deviceRack.controller.synthDecay).toFixed(2) + " s" : ""
                onValueModified: function(v) {
                    deviceRack.controller.setSynthDecay(Math.max(0.005, v * v * 4.0))
                }
            }
            SaudadeKnob {
                label: "Sustain"
                value: deviceRack.controller ? deviceRack.controller.synthSustain : 0.65
                displayText: Math.round(value * 100) + "%"
                onValueModified: function(v) { deviceRack.controller.setSynthSustain(v) }
            }
            SaudadeKnob {
                label: "Release"
                value: deviceRack.controller
                       ? Math.sqrt(deviceRack.controller.synthRelease / 6.0) : 0
                displayText: deviceRack.controller
                             ? Number(deviceRack.controller.synthRelease).toFixed(2) + " s" : ""
                onValueModified: function(v) {
                    deviceRack.controller.setSynthRelease(Math.max(0.005, v * v * 6.0))
                }
            }

            Rectangle {
                width: 1
                height: 64
                color: SaudadeTheme.lineNormal
                anchors.verticalCenter: parent.verticalCenter
            }

            SaudadeKnob {
                label: "Cutoff"
                value: deviceRack.controller
                       ? Math.log(deviceRack.controller.synthCutoff / 40)
                         / Math.log(18000 / 40) : 0.7
                displayText: deviceRack.controller
                             ? Math.round(deviceRack.controller.synthCutoff) + " Hz" : ""
                onValueModified: function(v) {
                    deviceRack.controller.setSynthCutoff(
                                40 * Math.pow(18000 / 40, v))
                }
            }
            SaudadeKnob {
                label: "Resonance"
                value: deviceRack.controller ? deviceRack.controller.synthResonance : 0.18
                displayText: Math.round(value * 100) + "%"
                onValueModified: function(v) { deviceRack.controller.setSynthResonance(v) }
            }
            SaudadeKnob {
                label: "Character"
                value: deviceRack.controller ? deviceRack.controller.synthCharacter : 0.78
                displayText: value < 0.5 ? "Triangle" : "Saw"
                onValueModified: function(v) { deviceRack.controller.setSynthCharacter(v) }
            }
        }
    }
}
