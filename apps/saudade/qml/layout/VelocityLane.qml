import QtQuick
import QtQuick.Controls
import "../theme"

Rectangle {
    id: velocityLane

    property var controller: null
    property real beatWidth: 180.0
    property real contentXOffset: 0.0
    property real keybedWidth: 60.0

    height: SaudadeTheme.velocityLaneHeight
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

        // Velocity Header Strip
        Rectangle {
            width: parent.width
            height: 24
            color: SaudadeTheme.bgWorkspace
            border.width: 0

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: SaudadeTheme.lineSoft
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Rectangle {
                    width: velTabLabel.implicitWidth + 12
                    height: 18
                    radius: 2
                    color: SaudadeTheme.bgPanelRaised
                    border.width: 1
                    border.color: SaudadeTheme.lineNormal

                    Text {
                        id: velTabLabel
                        anchors.centerIn: parent
                        text: "Velocity"
                        font.family: SaudadeTheme.fontSans
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        color: SaudadeTheme.textPrimary
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Pitch Bend"
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 9
                    color: SaudadeTheme.textMuted
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Aftertouch"
                    font.family: SaudadeTheme.fontSans
                    font.pixelSize: 9
                    color: SaudadeTheme.textMuted
                }
            }

            // Stats
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: "MIN: 48"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    color: SaudadeTheme.textMuted
                }
                Text {
                    text: "|"
                    font.pixelSize: 8
                    color: SaudadeTheme.lineSoft
                }
                Text {
                    text: "AVG: 96"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    color: SaudadeTheme.textMuted
                }
                Text {
                    text: "|"
                    font.pixelSize: 8
                    color: SaudadeTheme.lineSoft
                }
                Text {
                    text: "MAX: 127"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                    color: SaudadeTheme.textPrimary
                }
            }
        }

        // Velocity Pins Stage
        Item {
            width: parent.width
            height: parent.height - 24

            // Left scale numbers (aligned with keybed)
            Rectangle {
                id: scaleCol
                width: velocityLane.keybedWidth
                height: parent.height
                color: SaudadeTheme.bgCanvas
                border.width: 0

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: SaudadeTheme.lineSoft
                }

                Column {
                    anchors.fill: parent
                    anchors.rightMargin: 6
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: (parent.height - 8 - 5 * 10) / 4

                    Repeater {
                        model: ["127", "96", "64", "32", "0"]
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            text: modelData
                            font.family: SaudadeTheme.fontMono
                            font.pixelSize: 8
                            color: SaudadeTheme.textGhost
                        }
                    }
                }
            }

            // Stalks Canvas (Scrolled in sync with PianoRoll)
            Item {
                anchors.left: scaleCol.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                clip: true

                // Horizontal reference grid lines
                Column {
                    anchors.fill: parent
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: (parent.height - 8) / 4

                    Repeater {
                        model: 4
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: SaudadeTheme.lineSoft
                            opacity: 0.4
                        }
                    }
                }

                // Sub-container synced to piano roll scroll position
                Item {
                    id: scrolledPins
                    x: -velocityLane.contentXOffset
                    y: 0
                    width: 4000
                    height: parent.height

                    // Dynamic Velocity Pins generated from active sequence
                    // Whenever notesChanged is emitted, we redraw the stalks
                    Canvas {
                        id: pinsCanvas
                        anchors.fill: parent

                        function redraw() {
                            requestPaint();
                        }

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            ctx.clearRect(0, 0, width, height);

                            if (!velocityLane.controller) return;

                            var patLen = velocityLane.controller.patternLength;
                            var curBeat = velocityLane.controller.currentBeat;
                            var h = height;

                            // Draw subtle beat lines in velocity lane
                            ctx.strokeStyle = "#20232B";
                            ctx.lineWidth = 1;
                            for (var b = 0; b <= patLen * 4; b++) {
                                var x = b * (velocityLane.beatWidth * 0.25);
                                ctx.beginPath();
                                ctx.moveTo(x, 0);
                                ctx.lineTo(x, h);
                                ctx.stroke();
                            }

                            // Sample pins representation across beats
                            var pinBeats = [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5];
                            var pinValues = [0.8, 0.7, 0.95, 0.6, 0.85, 0.75, 0.9, 0.65];

                            for (var i = 0; i < pinBeats.length; i++) {
                                var px = pinBeats[i] * velocityLane.beatWidth + 8;
                                var val = pinValues[i];
                                var barH = val * (h - 16);
                                var py = h - barH;

                                // Stalk
                                ctx.strokeStyle = (i === 2) ? "#F1F0EC" : "#8FA5BA";
                                ctx.lineWidth = (i === 2) ? 2 : 1.5;
                                ctx.beginPath();
                                ctx.moveTo(px, h);
                                ctx.lineTo(px, py);
                                ctx.stroke();

                                // Machined Cap
                                ctx.fillStyle = (i === 2) ? "#F1F0EC" : "#8FA5BA";
                                ctx.beginPath();
                                ctx.arc(px, py, (i === 2) ? 3.5 : 2.5, 0, 2 * Math.PI);
                                ctx.fill();
                            }

                            // Playhead Guide Line
                            var playheadX = curBeat * velocityLane.beatWidth;
                            ctx.strokeStyle = "#D6B49A";
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            ctx.moveTo(playheadX, 0);
                            ctx.lineTo(playheadX, h);
                            ctx.stroke();
                        }
                    }

                    Connections {
                        target: velocityLane.controller
                        function onNotesChanged() { pinsCanvas.redraw(); }
                        function onCurrentBeatChanged() { pinsCanvas.redraw(); }
                    }
                }
            }
        }
    }
}
