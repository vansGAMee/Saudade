import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: subContext

    property string activeView: "PIANO ROLL" // "ARRANGEMENT", "PIANO ROLL", "MIXER", "DEVICES"
    property string activeTrackName: "Track 01 [PolySynth Lead]"
    property string activeClipName: "Pattern 01 [Main Motif]"
    property string snapSetting: "1/16"

    signal viewSelected(string viewName)

    height: SaudadeTheme.subContextHeight
    color: SaudadeTheme.bgWorkspace

    // Bottom hairline separator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineSoft
    }

    // Left Breadcrumbs
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Song: Vektor_Overdrive"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            color: SaudadeTheme.textMuted
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "/"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            color: SaudadeTheme.lineNormal
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: subContext.activeTrackName
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            font.weight: Font.Medium
            color: SaudadeTheme.textSecondary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "/"
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            color: SaudadeTheme.lineNormal
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: subContext.activeClipName
            font.family: SaudadeTheme.fontSans
            font.pixelSize: SaudadeTheme.textSmall
            font.weight: Font.DemiBold
            color: SaudadeTheme.accentSelection
        }
    }

    // Center Workspace Mode Switcher Tabs
    Row {
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: ["ARRANGEMENT", "PIANO ROLL", "MIXER", "DEVICES"]

            Rectangle {
                id: tabBtn
                width: tabLabel.implicitWidth + 16
                height: 22
                radius: SaudadeTheme.radiusSm
                color: subContext.activeView === modelData
                       ? SaudadeTheme.bgPanelRaised
                       : (tabMouse.containsMouse ? SaudadeTheme.bgControlHover : "transparent")

                border.width: 1
                border.color: subContext.activeView === modelData ? SaudadeTheme.lineNormal : "transparent"

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: modelData
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 10
                    font.weight: subContext.activeView === modelData ? Font.DemiBold : Font.Normal
                    font.letterSpacing: 0.5
                    color: subContext.activeView === modelData
                           ? SaudadeTheme.textPrimary
                           : (tabMouse.containsMouse ? SaudadeTheme.textSecondary : SaudadeTheme.textMuted)
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        subContext.activeView = modelData;
                        subContext.viewSelected(modelData);
                    }
                }
            }
        }
    }

    // Right Parameter Chips
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "SNAP:"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 9
                color: SaudadeTheme.textMuted
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: subContext.snapSetting
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: SaudadeTheme.textPrimary
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 12
            color: SaudadeTheme.lineSoft
        }

        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "QUANT:"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 9
                color: SaudadeTheme.textMuted
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "100%"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 10
                color: SaudadeTheme.textSecondary
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 12
            color: SaudadeTheme.lineSoft
        }

        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "LINK:"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 9
                color: SaudadeTheme.textMuted
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "ACTIVE"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: SaudadeTheme.accentSelection
            }
        }
    }
}
