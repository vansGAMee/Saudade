import QtQuick
import "../theme"

Rectangle {
    id: footer
    property var controller: null

    height: SaudadeTheme.footerHeight
    color: SaudadeTheme.bgCanvas

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineNormal
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.55
        elide: Text.ElideMiddle
        text: {
            if (!footer.controller) return "Untitled"
            if (footer.controller.lastError.length > 0)
                return footer.controller.lastError
            var location = footer.controller.projectPath.length > 0
                    ? footer.controller.projectPath : "Untitled project"
            return footer.controller.projectDirty ? location + " — unsaved changes" : location
        }
        font.family: footer.controller && footer.controller.lastError.length > 0
                     ? SaudadeTheme.fontSans : SaudadeTheme.fontMono
        font.pixelSize: 9
        color: footer.controller && footer.controller.lastError.length > 0
               ? SaudadeTheme.accentWarning : SaudadeTheme.textMuted
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "Space Play/Stop    Ctrl+S Save    Ctrl+I Import    Ctrl+E Export"
        font.family: SaudadeTheme.fontSans
        font.pixelSize: 9
        color: SaudadeTheme.textMuted
    }
}
