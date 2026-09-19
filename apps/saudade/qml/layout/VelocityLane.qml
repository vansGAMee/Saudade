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

            }

            readonly property var velStats: velocityLane.controller ? velocityLane.controller.getVelocityStats() : ({min: 0, avg: 0, max: 0})

            // Stats
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: "MIN: " + parent.parent.velStats.min
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
                    text: "AVG: " + parent.parent.velStats.avg
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
                    text: "MAX: " + parent.parent.velStats.max
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

                            // Draw subtle beat lines
                            ctx.strokeStyle = "#20232B";
                            ctx.lineWidth = 1;
                            for (var b = 0; b <= patLen * 4; b++) {
                                var x = b * (velocityLane.beatWidth * 0.25);
                                ctx.beginPath();
                                ctx.moveTo(x, 0);
                                ctx.lineTo(x, h);
                                ctx.stroke();
                            }

                            // Render real note velocity stalks
                            var notes = velocityLane.controller.getNotesData();
                            for (var i = 0; i < notes.length; i++) {
                                var note = notes[i];
                                var px = note.start * velocityLane.beatWidth + 4;
                                var val = note.velocity;
                                var barH = val * (h - 16);
                                var py = h - barH;

                                var isSel = note.selected;

                                // Stalk line
                                ctx.strokeStyle = isSel ? "#F1EEE7" : "#8FA5BA";
                                ctx.lineWidth = isSel ? 2.5 : 1.5;
                                ctx.beginPath();
                                ctx.moveTo(px, h);
                                ctx.lineTo(px, py);
                                ctx.stroke();

                                // Machined Cap
                                ctx.fillStyle = isSel ? "#FFFFFF" : "#8FA5BA";
                                ctx.beginPath();
                                ctx.arc(px, py, isSel ? 3.5 : 2.5, 0, 2 * Math.PI);
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

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        property var draggedNote: null
                        property bool gestureStarted: false

                        function updateVelocityAt(mouseX, mouseY) {
                            if (!velocityLane.controller) return;
                            var notes = velocityLane.controller.getNotesData();
                            var h = height;
                            var newVel = Math.min(1.0, Math.max(0.01, (h - mouseY) / Math.max(1.0, h - 16)));

                            if (!draggedNote) {
                                var bestDist = 20.0;
                                for (var i = 0; i < notes.length; i++) {
                                    var nx = notes[i].start * velocityLane.beatWidth + 4;
                                    var d = Math.abs(mouseX - nx);
                                    if (d < bestDist) {
                                        bestDist = d;
                                        draggedNote = notes[i];
                                    }
                                }
                            }

                            if (draggedNote) {
                                if (!gestureStarted) {
                                    velocityLane.controller.beginVelocityGesture(draggedNote.id);
                                    gestureStarted = true;
                                }
                                velocityLane.controller.previewVelocityGesture(newVel);
                                velocityLane.controller.auditionNoteOn(draggedNote.pitch, newVel);
                                pinsCanvas.redraw();
                            }
                        }

                        onPressed: (mouse) => {
                            draggedNote = null;
                            gestureStarted = false;
                            updateVelocityAt(mouse.x, mouse.y);
                        }

                        onPositionChanged: (mouse) => {
                            if (pressed) {
                                updateVelocityAt(mouse.x, mouse.y);
                            }
                        }

                        onReleased: {
                            if (velocityLane.controller) {
                                velocityLane.controller.auditionNoteOff();
                                velocityLane.controller.commitVelocityGesture();
                            }
                            draggedNote = null;
                            gestureStarted = false;
                        }
                    }

                    Connections {
                        target: velocityLane.controller
                        function onNotesChanged() { pinsCanvas.redraw(); }
                        function onSelectionChanged() { pinsCanvas.redraw(); }
                        function onCurrentBeatChanged() { pinsCanvas.redraw(); }
                    }
                }
            }
        }
    }
}
