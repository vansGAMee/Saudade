import QtQuick
import "../theme"

Rectangle {
    id: subContext

    property var controller: null
    property string activeView: "PIANO ROLL"
    signal viewSelected(string viewName)

    height: SaudadeTheme.subContextHeight
    color: SaudadeTheme.bgWorkspace

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineSoft
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: subContext.activeView === "PIANO ROLL"
              ? "PATTERN EDITOR" : subContext.activeView
        font.family: SaudadeTheme.fontSans
        font.pixelSize: 10
        font.weight: Font.DemiBold
        color: SaudadeTheme.textSecondary
    }

    Row {
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: ["ARRANGEMENT", "PIANO ROLL", "MIXER", "DEVICES"]
            delegate: Rectangle {
                id: tabButton
                required property string modelData
                width: label.implicitWidth + 16
                height: 22
                radius: SaudadeTheme.radiusSm
                color: subContext.activeView === modelData
                       ? SaudadeTheme.bgPanelRaised
                       : (mouse.containsMouse ? SaudadeTheme.bgControlHover : "transparent")
                border.width: 1
                border.color: subContext.activeView === modelData
                              ? SaudadeTheme.lineNormal : "transparent"

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 10
                    font.weight: subContext.activeView === modelData
                                 ? Font.DemiBold : Font.Normal
                    color: subContext.activeView === modelData
                           ? SaudadeTheme.textPrimary : SaudadeTheme.textMuted
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: subContext.viewSelected(modelData)
                }
            }
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "SNAP " + (subContext.controller
                         ? subContext.controller.snapStep : "1/16")
        font.family: SaudadeTheme.fontMono
        font.pixelSize: 9
        color: SaudadeTheme.textMuted
    }
}
