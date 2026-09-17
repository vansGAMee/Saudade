import QtQuick
import QtQuick.Controls
import saudade.ui 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1024
    height: 720
    minimumWidth: 700
    minimumHeight: 480
    title: "Saudade"
    color: "#121216"

    // Top Control Bar
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        color: "#18181f"

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#282935"
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 20

            Text {
                text: "SAUDADE"
                font.pixelSize: 15
                font.bold: true
                color: "#f4f4f5"
                anchors.verticalCenter: parent.verticalCenter
            }

            // Play Button
            Rectangle {
                id: playBtn
                width: 72
                height: 30
                radius: 4
                color: editorController.isPlaying ? "#2563eb" : "#27272a"
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "PLAY"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#ffffff"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: editorController.play()
                }
            }

            // Stop Button
            Rectangle {
                id: stopBtn
                width: 72
                height: 30
                radius: 4
                color: !editorController.isPlaying ? "#3f3f46" : "#27272a"
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "STOP"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#ffffff"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: editorController.stop()
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 24

            Text {
                text: editorController.bpm.toFixed(0) + " BPM"
                font.pixelSize: 13
                font.bold: true
                color: "#a1a1aa"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "Beat: " + editorController.currentBeat.toFixed(2)
                font.pixelSize: 13
                font.family: "Monospace"
                color: "#71717a"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Main Piano Roll Area
    Item {
        id: centerArea
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property int minPitch: 48
        readonly property int maxPitch: 84
        readonly property real rowHeight: 20.0
        readonly property real beatWidth: 180.0
        readonly property real contentHeightCalc: (maxPitch - minPitch + 1) * rowHeight

        // Left Piano Keys Strip (Pinned horizontally, synced vertically)
        Item {
            id: keysContainer
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 56
            clip: true

            PianoKeysItem {
                id: pianoKeys
                x: 0
                y: -pianoRollFlickable.contentY
                width: keysContainer.width
                height: centerArea.contentHeightCalc
                rowHeight: centerArea.rowHeight
                minPitch: centerArea.minPitch
                maxPitch: centerArea.maxPitch
            }
        }

        // Piano Roll Scrollable View
        Flickable {
            id: pianoRollFlickable
            anchors.left: keysContainer.right
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            contentWidth: Math.max(width, editorController.patternLength * centerArea.beatWidth)
            contentHeight: centerArea.contentHeightCalc

            PianoRollItem {
                id: pianoRoll
                objectName: "pianoRoll"
                width: pianoRollFlickable.contentWidth
                height: centerArea.contentHeightCalc
                controller: editorController
                rowHeight: centerArea.rowHeight
                beatWidth: centerArea.beatWidth
                minPitch: centerArea.minPitch
                maxPitch: centerArea.maxPitch
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
            }
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
