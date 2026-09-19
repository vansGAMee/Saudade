import QtQuick
import QtQuick.Controls
import "../theme"

Rectangle {
    id: control

    property string iconName: "play" // "play", "stop", "record", "loop", "rewind", "undo", "redo", "select", "pencil", "slice", "eraser", "search", "close", "plus", "minus", "settings", "folder", "zoom_in", "zoom_out"
    property string variant: "secondary" // "primary", "secondary", "subtle", "record", "active"
    property bool checkable: false
    property bool checked: false
    property real iconSize: 14
    property bool circular: false
    property string tooltipText: ""

    signal clicked()

    implicitWidth: circular ? 32 : 28
    implicitHeight: circular ? 32 : 28
    radius: circular ? SaudadeTheme.radiusFull : SaudadeTheme.radiusMd

    readonly property bool isHovered: mouseArea.containsMouse
    readonly property bool isPressed: mouseArea.pressed

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

    readonly property color fgColor: {
        if (!enabled) return SaudadeTheme.textGhost;
        if (variant === "primary" || (checkable && checked && variant !== "record")) {
            return SaudadeTheme.bgCanvas;
        }
        if (variant === "record") {
            return checked ? SaudadeTheme.textPrimary : SaudadeTheme.accentRecord;
        }
        return isHovered ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary;
    }

    Canvas {
        id: iconCanvas
        anchors.centerIn: parent
        width: control.iconSize
        height: control.iconSize

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            ctx.fillStyle = control.fgColor;
            ctx.strokeStyle = control.fgColor;
            ctx.lineWidth = 1.5;
            ctx.lineCap = "round";
            ctx.lineJoin = "round";

            var w = width;
            var h = height;

            switch (control.iconName) {
            case "play":
                ctx.beginPath();
                ctx.moveTo(w * 0.25, h * 0.15);
                ctx.lineTo(w * 0.85, h * 0.5);
                ctx.lineTo(w * 0.25, h * 0.85);
                ctx.closePath();
                ctx.fill();
                break;

            case "stop":
                ctx.beginPath();
                ctx.rect(w * 0.2, h * 0.2, w * 0.6, h * 0.6);
                ctx.fill();
                break;

            case "record":
                ctx.beginPath();
                ctx.arc(w * 0.5, h * 0.5, w * 0.35, 0, 2 * Math.PI);
                if (control.checked) {
                    ctx.fill();
                } else {
                    ctx.stroke();
                }
                break;

            case "loop":
                ctx.beginPath();
                // top arrow going right
                ctx.moveTo(w * 0.2, h * 0.35);
                ctx.lineTo(w * 0.75, h * 0.35);
                ctx.lineTo(w * 0.6, h * 0.18);
                // bottom arrow going left
                ctx.moveTo(w * 0.8, h * 0.65);
                ctx.lineTo(w * 0.25, h * 0.65);
                ctx.lineTo(w * 0.4, h * 0.82);
                ctx.stroke();
                break;

            case "rewind":
                // vertical bar on left
                ctx.beginPath();
                ctx.moveTo(w * 0.2, h * 0.2);
                ctx.lineTo(w * 0.2, h * 0.8);
                ctx.stroke();
                // left triangle
                ctx.beginPath();
                ctx.moveTo(w * 0.8, h * 0.2);
                ctx.lineTo(w * 0.35, h * 0.5);
                ctx.lineTo(w * 0.8, h * 0.8);
                ctx.closePath();
                ctx.fill();
                break;

            case "undo":
                ctx.beginPath();
                ctx.arc(w * 0.55, h * 0.55, w * 0.35, Math.PI, 1.75 * Math.PI, false);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.1, h * 0.4);
                ctx.lineTo(w * 0.2, h * 0.55);
                ctx.lineTo(w * 0.35, h * 0.45);
                ctx.stroke();
                break;

            case "redo":
                ctx.beginPath();
                ctx.arc(w * 0.45, h * 0.55, w * 0.35, 0, 1.25 * Math.PI, true);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.9, h * 0.4);
                ctx.lineTo(w * 0.8, h * 0.55);
                ctx.lineTo(w * 0.65, h * 0.45);
                ctx.stroke();
                break;

            case "select":
                ctx.beginPath();
                ctx.moveTo(w * 0.15, h * 0.1);
                ctx.lineTo(w * 0.4, h * 0.85);
                ctx.lineTo(w * 0.55, h * 0.6);
                ctx.lineTo(w * 0.85, h * 0.7);
                ctx.lineTo(w * 0.75, h * 0.5);
                ctx.lineTo(w * 0.9, h * 0.4);
                ctx.closePath();
                ctx.fill();
                break;

            case "pencil":
                ctx.beginPath();
                ctx.moveTo(w * 0.2, h * 0.8);
                ctx.lineTo(w * 0.75, h * 0.25);
                ctx.lineTo(w * 0.85, h * 0.35);
                ctx.lineTo(w * 0.3, h * 0.9);
                ctx.closePath();
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.2, h * 0.8);
                ctx.lineTo(w * 0.12, h * 0.92);
                ctx.lineTo(w * 0.3, h * 0.9);
                ctx.closePath();
                ctx.fill();
                break;

            case "slice":
                ctx.beginPath();
                ctx.moveTo(w * 0.2, h * 0.2);
                ctx.lineTo(w * 0.8, h * 0.8);
                ctx.moveTo(w * 0.8, h * 0.2);
                ctx.lineTo(w * 0.2, h * 0.8);
                ctx.stroke();
                break;

            case "eraser":
                ctx.beginPath();
                ctx.moveTo(w * 0.35, h * 0.2);
                ctx.lineTo(w * 0.85, h * 0.2);
                ctx.lineTo(w * 0.85, h * 0.8);
                ctx.lineTo(w * 0.35, h * 0.8);
                ctx.lineTo(w * 0.15, h * 0.5);
                ctx.closePath();
                ctx.stroke();
                break;

            case "search":
                ctx.beginPath();
                ctx.arc(w * 0.42, h * 0.42, w * 0.28, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.62, h * 0.62);
                ctx.lineTo(w * 0.88, h * 0.88);
                ctx.stroke();
                break;

            case "close":
                ctx.beginPath();
                ctx.moveTo(w * 0.25, h * 0.25);
                ctx.lineTo(w * 0.75, h * 0.75);
                ctx.moveTo(w * 0.75, h * 0.25);
                ctx.lineTo(w * 0.25, h * 0.75);
                ctx.stroke();
                break;

            case "plus":
                ctx.beginPath();
                ctx.moveTo(w * 0.5, h * 0.2);
                ctx.lineTo(w * 0.5, h * 0.8);
                ctx.moveTo(w * 0.2, h * 0.5);
                ctx.lineTo(w * 0.8, h * 0.5);
                ctx.stroke();
                break;

            case "minus":
                ctx.beginPath();
                ctx.moveTo(w * 0.2, h * 0.5);
                ctx.lineTo(w * 0.8, h * 0.5);
                ctx.stroke();
                break;

            case "zoom_in":
                ctx.beginPath();
                ctx.arc(w * 0.42, h * 0.42, w * 0.28, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.62, h * 0.62);
                ctx.lineTo(w * 0.88, h * 0.88);
                ctx.moveTo(w * 0.42, h * 0.28);
                ctx.lineTo(w * 0.42, h * 0.56);
                ctx.moveTo(w * 0.28, h * 0.42);
                ctx.lineTo(w * 0.56, h * 0.42);
                ctx.stroke();
                break;

            case "zoom_out":
                ctx.beginPath();
                ctx.arc(w * 0.42, h * 0.42, w * 0.28, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.62, h * 0.62);
                ctx.lineTo(w * 0.88, h * 0.88);
                ctx.moveTo(w * 0.28, h * 0.42);
                ctx.lineTo(w * 0.56, h * 0.42);
                ctx.stroke();
                break;

            case "settings":
                ctx.beginPath();
                ctx.arc(w * 0.5, h * 0.5, w * 0.2, 0, 2 * Math.PI);
                ctx.stroke();
                for (var i = 0; i < 6; i++) {
                    var angle = i * (Math.PI / 3);
                    var x1 = w * 0.5 + Math.cos(angle) * (w * 0.28);
                    var y1 = h * 0.5 + Math.sin(angle) * (h * 0.28);
                    var x2 = w * 0.5 + Math.cos(angle) * (w * 0.42);
                    var y2 = h * 0.5 + Math.sin(angle) * (h * 0.42);
                    ctx.beginPath();
                    ctx.moveTo(x1, y1);
                    ctx.lineTo(x2, y2);
                    ctx.stroke();
                }
                break;

            case "metronome":
                ctx.beginPath();
                ctx.moveTo(w * 0.3, h * 0.85);
                ctx.lineTo(w * 0.45, h * 0.2);
                ctx.lineTo(w * 0.55, h * 0.2);
                ctx.lineTo(w * 0.7, h * 0.85);
                ctx.closePath();
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.5, h * 0.75);
                ctx.lineTo(w * 0.68, h * 0.28);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(w * 0.64, h * 0.38, 2.0, 0, 2 * Math.PI);
                ctx.fill();
                break;

            default:
                ctx.beginPath();
                ctx.arc(w * 0.5, h * 0.5, w * 0.3, 0, 2 * Math.PI);
                ctx.stroke();
                break;
            }
        }
    }

    onFgColorChanged: iconCanvas.requestPaint()
    onIconNameChanged: iconCanvas.requestPaint()
    onWidthChanged: iconCanvas.requestPaint()
    onHeightChanged: iconCanvas.requestPaint()

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
