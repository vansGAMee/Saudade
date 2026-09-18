import QtQuick
import QtQuick.Controls
import "../theme"

Rectangle {
    id: control

    property string text: ""
    property string variant: "secondary" // "primary", "secondary", "subtle", "record", "active"
    property bool checkable: false
    property bool checked: false
    property bool compact: false
    property alias cursorShape: mouseArea.cursorShape
    property string tooltipText: ""

    signal clicked()

    implicitWidth: Math.max(compact ? 24 : 32, label.implicitWidth + (compact ? 12 : 20))
    implicitHeight: compact ? 24 : 30
    radius: SaudadeTheme.radiusMd

    readonly property bool isHovered: mouseArea.containsMouse
    readonly property bool isPressed: mouseArea.pressed

    // Background color computation based on variant and state
    color: {
        if (!enabled) return SaudadeTheme.bgControl;
        if (variant === "primary" || (checkable && checked && variant !== "record")) {
            return isPressed ? "#DCDAD3" : (isHovered ? "#FFFFFF" : SaudadeTheme.textPrimary);
        }
        if (variant === "record") {
            if (checked) {
                return isPressed ? "#B84848" : (isHovered ? "#E06868" : SaudadeTheme.accentRecord);
            }
            return isPressed ? SaudadeTheme.bgControlPressed : (isHovered ? SaudadeTheme.bgControlHover : SaudadeTheme.bgControl);
        }
        if (variant === "subtle") {
            return isPressed ? SaudadeTheme.bgControlPressed : (isHovered ? SaudadeTheme.bgControlHover : "transparent");
        }
        // secondary
        return isPressed ? SaudadeTheme.bgControlPressed : (isHovered ? SaudadeTheme.bgControlHover : SaudadeTheme.bgControl);
    }

    border.width: 1
    border.color: {
        if (variant === "record") {
            return checked ? SaudadeTheme.accentRecord : (isHovered ? SaudadeTheme.accentRecord : SaudadeTheme.lineNormal);
        }
        if (variant === "primary" || (checkable && checked)) {
            return SaudadeTheme.lineFocus;
        }
        if (variant === "subtle") {
            return isHovered ? SaudadeTheme.lineSoft : "transparent";
        }
        return isHovered ? SaudadeTheme.lineNormal : SaudadeTheme.lineSoft;
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: control.text
        font.family: SaudadeTheme.fontSans
        font.pixelSize: control.compact ? SaudadeTheme.textSmall : SaudadeTheme.textBody
        font.weight: (control.variant === "primary" || (control.checkable && control.checked)) ? Font.DemiBold : Font.Normal
        color: {
            if (!control.enabled) return SaudadeTheme.textGhost;
            if (control.variant === "primary" || (control.checkable && control.checked && control.variant !== "record")) {
                return SaudadeTheme.bgCanvas;
            }
            if (control.variant === "record") {
                return control.checked ? SaudadeTheme.textPrimary : SaudadeTheme.accentRecord;
            }
            return control.isHovered ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary;
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (control.checkable) {
                control.checked = !control.checked;
            }
            control.clicked();
        }
    }
}
