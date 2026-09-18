import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: knob

    property string label: "PARAM"
    property real value: 0.5 // 0.0 to 1.0
    property string displayText: Math.round(value * 100) + "%"
    property real defaultValue: 0.5
    property bool accentPointer: false
    property real knobSize: 42

    signal valueModified(real newValue)

    implicitWidth: knobSize + 16
    implicitHeight: knobSize + 28

    readonly property bool isHovered: mouseArea.containsMouse
    readonly property bool isPressed: mouseArea.pressed

    // Rotary Dial Container
    Rectangle {
        id: dial
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: knob.knobSize
        height: knob.knobSize
        radius: width * 0.5
        color: knob.isPressed ? SaudadeTheme.bgControlPressed : (knob.isHovered ? SaudadeTheme.bgControlHover : SaudadeTheme.bgControl)
        border.width: 1
        border.color: knob.isHovered ? SaudadeTheme.lineFocus : SaudadeTheme.lineNormal

        // Inset ring shadow / texture
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 4
            height: parent.height - 4
            radius: width * 0.5
            color: "transparent"
            border.width: 1
            border.color: SaudadeTheme.lineSoft
        }

        // Radial pointer tick line
        // Angle range: -135 deg to +135 deg (270 deg sweep)
        Item {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            rotation: -135 + knob.value * 270

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 3
                width: 2
                height: knob.knobSize * 0.26
                radius: 1
                color: knob.accentPointer ? SaudadeTheme.accentPlayhead : (knob.isHovered ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary)
            }
        }

        // Center value text
        Text {
            anchors.centerIn: parent
            text: knob.displayText
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            font.weight: Font.DemiBold
            color: knob.isHovered ? SaudadeTheme.textPrimary : SaudadeTheme.textSecondary
        }
    }

    // Label below dial
    Text {
        anchors.top: dial.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: knob.label
        font.family: SaudadeTheme.fontSans
        font.pixelSize: 9
        font.weight: Font.Medium
        font.capitalization: Font.AllUppercase
        color: knob.isHovered ? SaudadeTheme.textSecondary : SaudadeTheme.textMuted
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.SizeVerCursor
        preventStealing: true

        property real startY: 0
        property real startVal: 0

        onPressed: (mouse) => {
            startY = mouse.y;
            startVal = knob.value;
        }

        onPositionChanged: (mouse) => {
            if (pressed) {
                var dy = startY - mouse.y;
                var delta = dy / 120.0;
                var newVal = Math.max(0.0, Math.min(1.0, startVal + delta));
                knob.value = newVal;
                knob.valueModified(newVal);
            }
        }

        onDoubleClicked: {
            knob.value = knob.defaultValue;
            knob.valueModified(knob.defaultValue);
        }
    }
}
