import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: ribbon

    property string currentPitch: "C4"
    property int currentVel: 96
    property string currentLen: "1/4"
    property string currentGrid: "1/16"
    property string currentScale: "D Aeolian"
    property string activeTool: "pencil" // "select", "pencil", "slice", "eraser"
    property real zoomLevel: 1.0

    signal toolChanged(string toolName)
    signal quantizeRequested()
    signal humanizeRequested()
    signal zoomInRequested()
    signal zoomOutRequested()

    height: SaudadeTheme.toolRibbonHeight
    color: SaudadeTheme.bgPanel
    border.width: 0

    // Bottom hairline separator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineSoft
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        // PITCH
        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "PITCH"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 9
                font.weight: Font.Medium
                color: SaudadeTheme.textMuted
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 20
                radius: 2
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: SaudadeTheme.lineSoft
                Text {
                    anchors.centerIn: parent
                    text: ribbon.currentPitch
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    color: SaudadeTheme.textPrimary
                }
            }
        }

        // VEL
        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "VEL"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 9
                font.weight: Font.Medium
                color: SaudadeTheme.textMuted
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 20
                radius: 2
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: SaudadeTheme.lineSoft
                Text {
                    anchors.centerIn: parent
                    text: "" + ribbon.currentVel
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    color: SaudadeTheme.accentPlayhead
                }
            }
        }

        // LEN
        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "LEN"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 9
                font.weight: Font.Medium
                color: SaudadeTheme.textMuted
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 20
                radius: 2
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: SaudadeTheme.lineSoft
                Text {
                    anchors.centerIn: parent
                    text: ribbon.currentLen
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 10
                    color: SaudadeTheme.textSecondary
                }
            }
        }

        // GRID
        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "GRID"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 9
                font.weight: Font.Medium
                color: SaudadeTheme.textMuted
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 20
                radius: 2
                color: SaudadeTheme.bgCanvas
                border.width: 1
                border.color: SaudadeTheme.lineSoft
                Text {
                    anchors.centerIn: parent
                    text: ribbon.currentGrid
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 10
                    color: SaudadeTheme.textSecondary
                }
            }
        }

        // Hairline separator
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 14
            color: SaudadeTheme.lineSoft
        }

        // SCALE
        Row {
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "SCALE"
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 9
                font.weight: Font.Medium
                color: SaudadeTheme.textMuted
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: scaleText.implicitWidth + 12
                height: 20
                radius: 2
                color: SaudadeTheme.bgPanelRaised
                border.width: 1
                border.color: SaudadeTheme.lineNormal
                Text {
                    id: scaleText
                    anchors.centerIn: parent
                    text: ribbon.currentScale
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: SaudadeTheme.textPrimary
                }
            }
        }

        // Quick action chips
        SaudadeButton {
            text: "Fold"
            variant: "secondary"
            compact: true
            tooltipText: "Fold piano roll to active scale"
        }
        SaudadeButton {
            text: "Ghosts"
            variant: "secondary"
            checkable: true
            checked: true
            compact: true
            tooltipText: "Toggle ghost notes display"
        }
    }

    // Right Action & Tool Buttons
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // Editing Tools Group
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 26
            width: toolsRow.width + 6
            radius: SaudadeTheme.radiusSm
            color: SaudadeTheme.bgCanvas
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            Row {
                id: toolsRow
                anchors.centerIn: parent
                spacing: 2

                SaudadeIconButton {
                    iconName: "select"
                    variant: ribbon.activeTool === "select" ? "primary" : "subtle"
                    tooltipText: "Select Tool (V)"
                    width: 22
                    height: 22
                    iconSize: 12
                    onClicked: {
                        ribbon.activeTool = "select";
                        ribbon.toolChanged("select");
                    }
                }

                SaudadeIconButton {
                    iconName: "pencil"
                    variant: ribbon.activeTool === "pencil" ? "primary" : "subtle"
                    tooltipText: "Pencil / Draw (B)"
                    width: 22
                    height: 22
                    iconSize: 12
                    onClicked: {
                        ribbon.activeTool = "pencil";
                        ribbon.toolChanged("pencil");
                    }
                }

                SaudadeIconButton {
                    iconName: "slice"
                    variant: ribbon.activeTool === "slice" ? "primary" : "subtle"
                    tooltipText: "Razor / Slice (C)"
                    width: 22
                    height: 22
                    iconSize: 12
                    onClicked: {
                        ribbon.activeTool = "slice";
                        ribbon.toolChanged("slice");
                    }
                }

                SaudadeIconButton {
                    iconName: "eraser"
                    variant: ribbon.activeTool === "eraser" ? "primary" : "subtle"
                    tooltipText: "Eraser (E)"
                    width: 22
                    height: 22
                    iconSize: 12
                    onClicked: {
                        ribbon.activeTool = "eraser";
                        ribbon.toolChanged("eraser");
                    }
                }
            }
        }

        // Quantize & Humanize
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            SaudadeButton {
                text: "Quantize"
                variant: "secondary"
                compact: true
                tooltipText: "Quantize Note Starts (Q)"
                onClicked: ribbon.quantizeRequested()
            }

            SaudadeButton {
                text: "Humanize"
                variant: "secondary"
                compact: true
                tooltipText: "Subtle Timing & Velocity Variations"
                onClicked: ribbon.humanizeRequested()
            }
        }

        // Zoom Controls
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            height: 26
            width: zoomRow.width + 6
            radius: SaudadeTheme.radiusSm
            color: SaudadeTheme.bgCanvas
            border.width: 1
            border.color: SaudadeTheme.lineSoft

            Row {
                id: zoomRow
                anchors.centerIn: parent
                spacing: 2

                SaudadeIconButton {
                    iconName: "zoom_out"
                    variant: "subtle"
                    width: 20
                    height: 20
                    iconSize: 11
                    onClicked: ribbon.zoomOutRequested()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(ribbon.zoomLevel * 100) + "%"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 9
                    color: SaudadeTheme.textSecondary
                }

                SaudadeIconButton {
                    iconName: "zoom_in"
                    variant: "subtle"
                    width: 20
                    height: 20
                    iconSize: 11
                    onClicked: ribbon.zoomInRequested()
                }
            }
        }
    }
}
