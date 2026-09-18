import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Rectangle {
    id: deviceRack

    property bool collapsed: false
    property string activeTrackName: "TRACK 01: ANALOG POLY SYNTH"
    property string activePreset: "PolySynth_Lead_Overdrive_01"

    signal toggleCollapse()

    height: collapsed ? 28 : SaudadeTheme.deviceRackHeight
    color: SaudadeTheme.bgPanel
    border.width: 0

    // Top border separator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: SaudadeTheme.lineNormal
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // Device Rack Header
        Rectangle {
            width: parent.width
            height: 28
            color: SaudadeTheme.bgPanelRaised
            border.width: 0

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: SaudadeTheme.lineSoft
            }

            // Left Track Instrument Identity
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // Collapse toggle chevron
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: deviceRack.collapsed ? "▸" : "▾"
                    font.pixelSize: 10
                    color: SaudadeTheme.textSecondary

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: deviceRack.collapsed = !deviceRack.collapsed
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: deviceRack.activeTrackName
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: SaudadeTheme.textSmall
                    font.weight: Font.DemiBold
                    color: SaudadeTheme.textPrimary
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "/"
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: SaudadeTheme.textSmall
                    color: SaudadeTheme.textGhost
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "DEVICE CHAIN (3 INSERT MODULES)"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 9
                    color: SaudadeTheme.textMuted
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 12
                    color: SaudadeTheme.lineSoft
                }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        text: "PRESET:"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 8
                        color: SaudadeTheme.textMuted
                    }
                    Text {
                        text: deviceRack.activePreset
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        color: SaudadeTheme.accentPlayhead
                    }
                }
            }

            // Right Actions
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                SaudadeButton {
                    text: "Bypass All"
                    variant: "secondary"
                    compact: true
                }

                SaudadeButton {
                    text: "+ Add Device"
                    variant: "secondary"
                    compact: true
                }
            }
        }

        // Horizontal Rack Modules Scroll Area
        Flickable {
            visible: !deviceRack.collapsed
            width: parent.width
            height: parent.height - 28
            contentWidth: modulesRow.implicitWidth + 24
            contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Row {
                id: modulesRow
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                spacing: 10

                // ==========================================
                // MODULE 1: PolySynth / Ladder Filter & Drive
                // ==========================================
                Rectangle {
                    width: 320
                    height: 148
                    radius: SaudadeTheme.radiusMd
                    color: SaudadeTheme.bgWorkspace
                    border.width: 1
                    border.color: SaudadeTheme.lineNormal

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        // Module Header
                        Row {
                            width: parent.width
                            Text {
                                text: "PolySynth (Native DSP)"
                                font.family: SaudadeTheme.fontSans
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: SaudadeTheme.textPrimary
                            }
                            Item { width: 10; height: 1 }
                            Text {
                                text: "LADDER 24dB"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 8
                                color: SaudadeTheme.textGhost
                            }
                        }

                        // Knobs Row 1: Filter & Drive
                        Row {
                            spacing: 8
                            anchors.horizontalCenter: parent.horizontalCenter

                            SaudadeKnob {
                                label: "CUTOFF"
                                value: 0.65
                                displayText: "2.4k"
                                accentPointer: true
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "RESO"
                                value: 0.35
                                displayText: "35%"
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "DRIVE"
                                value: 0.18
                                displayText: "18%"
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "ATTACK"
                                value: 0.12
                                displayText: "12ms"
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "RELEASE"
                                value: 0.40
                                displayText: "240ms"
                                knobSize: 36
                            }
                        }
                    }
                }

                // ==========================================
                // MODULE 2: Parametric Stereo EQ (Visualizer)
                // ==========================================
                Rectangle {
                    width: 260
                    height: 148
                    radius: SaudadeTheme.radiusMd
                    color: SaudadeTheme.bgWorkspace
                    border.width: 1
                    border.color: SaudadeTheme.lineNormal

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        Row {
                            width: parent.width
                            Text {
                                text: "Parametric EQ"
                                font.family: SaudadeTheme.fontSans
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: SaudadeTheme.textPrimary
                            }
                            Item { width: 10; height: 1 }
                            Text {
                                text: "4-BAND STEREO"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 8
                                color: SaudadeTheme.textGhost
                            }
                        }

                        // EQ Display Canvas
                        Rectangle {
                            width: parent.width
                            height: 96
                            radius: 2
                            color: SaudadeTheme.bgCanvas
                            border.width: 1
                            border.color: SaudadeTheme.lineSoft

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    var w = width;
                                    var h = height;

                                    // Grid lines
                                    ctx.strokeStyle = "#20232B";
                                    ctx.lineWidth = 1;
                                    for (var x = w * 0.2; x < w; x += w * 0.2) {
                                        ctx.beginPath();
                                        ctx.moveTo(x, 0);
                                        ctx.lineTo(x, h);
                                        ctx.stroke();
                                    }
                                    ctx.beginPath();
                                    ctx.moveTo(0, h * 0.5);
                                    ctx.lineTo(w, h * 0.5);
                                    ctx.stroke();

                                    // Parametric Curve
                                    ctx.strokeStyle = "#8FA5BA";
                                    ctx.lineWidth = 2;
                                    ctx.beginPath();
                                    ctx.moveTo(0, h * 0.5);
                                    ctx.bezierCurveTo(w * 0.2, h * 0.5, w * 0.3, h * 0.25, w * 0.45, h * 0.35);
                                    ctx.bezierCurveTo(w * 0.6, h * 0.45, w * 0.7, h * 0.6, w * 0.85, h * 0.4);
                                    ctx.lineTo(w, h * 0.5);
                                    ctx.stroke();

                                    // Nodes
                                    var nodes = [
                                        { x: w * 0.45, y: h * 0.35 },
                                        { x: w * 0.85, y: h * 0.4 }
                                    ];
                                    for (var i = 0; i < nodes.length; i++) {
                                        ctx.fillStyle = "#E8E6DF";
                                        ctx.beginPath();
                                        ctx.arc(nodes[i].x, nodes[i].y, 3, 0, 2 * Math.PI);
                                        ctx.fill();
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // MODULE 3: Digital Stereo Delay
                // ==========================================
                Rectangle {
                    width: 220
                    height: 148
                    radius: SaudadeTheme.radiusMd
                    color: SaudadeTheme.bgWorkspace
                    border.width: 1
                    border.color: SaudadeTheme.lineNormal

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        Row {
                            width: parent.width
                            Text {
                                text: "Stereo Delay"
                                font.family: SaudadeTheme.fontSans
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: SaudadeTheme.textPrimary
                            }
                            Item { width: 10; height: 1 }
                            Text {
                                text: "BPM SYNC"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 8
                                color: SaudadeTheme.textGhost
                            }
                        }

                        Row {
                            spacing: 12
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.topMargin: 10

                            SaudadeKnob {
                                label: "TIME"
                                value: 0.375
                                displayText: "3/16"
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "FEEDBACK"
                                value: 0.45
                                displayText: "45%"
                                knobSize: 36
                            }
                            SaudadeKnob {
                                label: "MIX"
                                value: 0.25
                                displayText: "25%"
                                knobSize: 36
                            }
                        }
                    }
                }

                // ==========================================
                // Empty Insert Slot Card (+ Add Device)
                // ==========================================
                Rectangle {
                    width: 110
                    height: 148
                    radius: SaudadeTheme.radiusMd
                    color: "transparent"
                    border.width: 1
                    border.color: slotMouse.containsMouse ? SaudadeTheme.lineFocus : SaudadeTheme.lineSoft

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "+"
                            font.pixelSize: 18
                            color: slotMouse.containsMouse ? SaudadeTheme.textPrimary : SaudadeTheme.textGhost
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "INSERT"
                            font.family: SaudadeTheme.fontMono
                            font.pixelSize: 9
                            color: slotMouse.containsMouse ? SaudadeTheme.textPrimary : SaudadeTheme.textGhost
                        }
                    }

                    MouseArea {
                        id: slotMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    Behavior on height {
        NumberAnimation { duration: 140; easing.type: Easing.OutQuad }
    }
}
