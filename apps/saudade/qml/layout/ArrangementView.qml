import QtQuick
import QtQuick.Controls
import "../theme"
import "../components"

Item {
    id: arrangeView

    property var controller: null
    property real beatWidth: 52.0
    property real trackHeaderWidth: 210.0
    property real trackHeight: 64.0
    property int timelineBeats: 128
    property var selectedClipId: 0

    signal openPianoRollRequested()
    signal importMidiRequested()
    signal exportWavRequested()

    Rectangle { anchors.fill: parent; color: SaudadeTheme.bgWorkspace }

    Rectangle {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        color: SaudadeTheme.bgCanvas
        border.color: SaudadeTheme.lineSoft

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            SaudadeButton {
                text: "+ Track"
                compact: true
                tooltipText: "Create instrument track"
                onClicked: arrangeView.controller.addTrack("Instrument")
            }
            SaudadeButton {
                text: "+ Pattern"
                compact: true
                variant: "primary"
                tooltipText: "Create a 4-bar pattern at the playhead"
                onClicked: {
                    var tracks = arrangeView.controller.tracksData
                    if (tracks.length === 0) return
                    var trackId = tracks[0].id
                    for (var i = 0; i < tracks.length; ++i)
                        if (tracks[i].selected) trackId = tracks[i].id
                    var start = Math.round(arrangeView.controller.currentBeat * 4) / 4
                    arrangeView.selectedClipId =
                            arrangeView.controller.createPattern(trackId, start, 16.0)
                }
            }
            SaudadeButton {
                text: "Import MIDI"
                compact: true
                tooltipText: "Import a Standard MIDI File"
                onClicked: arrangeView.importMidiRequested()
            }
            SaudadeButton {
                text: "Export WAV"
                compact: true
                tooltipText: "Render project or active loop to stereo WAV"
                onClicked: arrangeView.exportWavRequested()
            }
            SaudadeButton {
                text: "Duplicate"
                compact: true
                enabled: arrangeView.selectedClipId !== 0
                tooltipText: "Duplicate selected clip (Ctrl+D)"
                onClicked: arrangeView.selectedClipId =
                                   arrangeView.controller.duplicateClip(arrangeView.selectedClipId)
            }
            SaudadeButton {
                text: "Delete"
                compact: true
                enabled: arrangeView.selectedClipId !== 0
                tooltipText: "Delete selected clip (Delete)"
                onClicked: {
                    arrangeView.controller.deleteClip(arrangeView.selectedClipId)
                    arrangeView.selectedClipId = 0
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "ZOOM"
                font.family: SaudadeTheme.fontMono
                font.pixelSize: 9
                color: SaudadeTheme.textMuted
            }
            SaudadeButton {
                text: "−"
                compact: true
                onClicked: arrangeView.beatWidth = Math.max(20, arrangeView.beatWidth - 8)
            }
            SaudadeButton {
                text: "+"
                compact: true
                onClicked: arrangeView.beatWidth = Math.min(140, arrangeView.beatWidth + 8)
            }
        }
    }

    Rectangle {
        id: rulerHeader
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        width: arrangeView.trackHeaderWidth
        height: 26
        color: SaudadeTheme.bgCanvas
        border.color: SaudadeTheme.lineNormal

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: (arrangeView.controller ? arrangeView.controller.tracksData.length : 0) + " TRACKS"
            font.family: SaudadeTheme.fontMono
            font.pixelSize: 9
            color: SaudadeTheme.textMuted
        }
    }

    Flickable {
        id: rulerFlick
        anchors.top: toolbar.bottom
        anchors.left: rulerHeader.right
        anchors.right: parent.right
        height: 26
        contentWidth: arrangeView.timelineBeats * arrangeView.beatWidth
        contentHeight: height
        contentX: timelineFlick.contentX
        interactive: false
        clip: true

        Item {
            width: rulerFlick.contentWidth
            height: rulerFlick.height
            Repeater {
                model: arrangeView.timelineBeats / 4
                delegate: Item {
                    x: index * 4 * arrangeView.beatWidth
                    width: 4 * arrangeView.beatWidth
                    height: rulerFlick.height
                    Rectangle {
                        width: 1
                        height: parent.height
                        color: SaudadeTheme.lineNormal
                    }
                    Text {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: (index + 1) + ".1"
                        font.family: SaudadeTheme.fontMono
                        font.pixelSize: 10
                        color: SaudadeTheme.textSecondary
                    }
                }
            }
        }
    }

    Flickable {
        id: headerFlick
        anchors.top: rulerHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: arrangeView.trackHeaderWidth
        contentWidth: width
        contentHeight: Math.max(height, trackColumn.height)
        contentY: timelineFlick.contentY
        interactive: false
        clip: true

        Column {
            id: trackColumn
            width: parent.width
            Repeater {
                model: arrangeView.controller ? arrangeView.controller.tracksData : []
                delegate: Rectangle {
                    required property var modelData
                    width: arrangeView.trackHeaderWidth
                    height: arrangeView.trackHeight
                    color: modelData.selected ? SaudadeTheme.bgPanelRaised : SaudadeTheme.bgPanel
                    border.color: modelData.selected ? SaudadeTheme.lineFocus : SaudadeTheme.lineSoft

                    Column {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 7
                        Row {
                            spacing: 7
                            Text {
                                text: String(modelData.index + 1).padStart(2, "0")
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 9
                                color: SaudadeTheme.textMuted
                            }
                            Text {
                                width: 145
                                text: modelData.name
                                elide: Text.ElideRight
                                font.family: SaudadeTheme.fontSans
                                font.pixelSize: 11
                                font.weight: modelData.selected ? Font.DemiBold : Font.Medium
                                color: SaudadeTheme.textPrimary
                            }
                        }
                        Row {
                            spacing: 5
                            SaudadeButton {
                                text: "M"
                                compact: true
                                checkable: true
                                checked: modelData.muted
                                tooltipText: "Mute track"
                                onClicked: arrangeView.controller.setTrackMute(modelData.id, checked)
                            }
                            SaudadeButton {
                                text: "S"
                                compact: true
                                checkable: true
                                checked: modelData.solo
                                tooltipText: "Solo track"
                                onClicked: arrangeView.controller.setTrackSolo(modelData.id, checked)
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Number(modelData.gainDb).toFixed(1) + " dB"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 9
                                color: SaudadeTheme.textSecondary
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        onClicked: arrangeView.controller.selectTrack(modelData.id)
                    }
                }
            }
        }
    }

    Flickable {
        id: timelineFlick
        anchors.top: rulerHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: rulerHeader.right
        anchors.right: parent.right
        contentWidth: arrangeView.timelineBeats * arrangeView.beatWidth
        contentHeight: Math.max(height,
                                (arrangeView.controller ? arrangeView.controller.tracksData.length : 1)
                                * arrangeView.trackHeight)
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Item {
            id: timelineContent
            width: timelineFlick.contentWidth
            height: timelineFlick.contentHeight

            Canvas {
                id: gridCanvas
                anchors.fill: parent
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: arrangeView
                    function onBeatWidthChanged() { gridCanvas.requestPaint() }
                }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.lineWidth = 1
                    for (var beat = 0; beat <= arrangeView.timelineBeats; ++beat) {
                        ctx.strokeStyle = (beat % 4 === 0)
                                ? SaudadeTheme.lineNormal : SaudadeTheme.lineSoft
                        ctx.beginPath()
                        ctx.moveTo(beat * arrangeView.beatWidth + 0.5, 0)
                        ctx.lineTo(beat * arrangeView.beatWidth + 0.5, height)
                        ctx.stroke()
                    }
                    ctx.strokeStyle = SaudadeTheme.lineSoft
                    var count = arrangeView.controller ? arrangeView.controller.tracksData.length : 1
                    for (var row = 0; row <= count; ++row) {
                        ctx.beginPath()
                        ctx.moveTo(0, row * arrangeView.trackHeight + 0.5)
                        ctx.lineTo(width, row * arrangeView.trackHeight + 0.5)
                        ctx.stroke()
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onDoubleClicked: {
                    var tracks = arrangeView.controller.tracksData
                    if (tracks.length === 0) return
                    var row = Math.max(0, Math.min(tracks.length - 1,
                                                  Math.floor(mouse.y / arrangeView.trackHeight)))
                    var beat = Math.max(0, Math.round(mouse.x / arrangeView.beatWidth * 4) / 4)
                    arrangeView.selectedClipId =
                            arrangeView.controller.createPattern(tracks[row].id, beat, 16.0)
                }
            }

            Repeater {
                model: arrangeView.controller ? arrangeView.controller.clipsData : []
                delegate: Rectangle {
                    id: clipItem
                    required property var modelData

                    x: modelData.startBeat * arrangeView.beatWidth
                    y: modelData.trackIndex * arrangeView.trackHeight + 4
                    width: Math.max(24, modelData.durationBeats * arrangeView.beatWidth)
                    height: arrangeView.trackHeight - 8
                    radius: SaudadeTheme.radiusMd
                    color: modelData.selected ? SaudadeTheme.bgControlHover
                                              : SaudadeTheme.bgPanelRaised
                    border.width: modelData.selected ? 2 : 1
                    border.color: modelData.selected ? SaudadeTheme.lineFocus
                                                    : SaudadeTheme.lineNormal

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: resizeHandle.left
                        anchors.rightMargin: 4
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        text: modelData.name
                        elide: Text.ElideRight
                        font.family: SaudadeTheme.fontSans
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: SaudadeTheme.textPrimary
                    }

                    MouseArea {
                        anchors.left: parent.left
                        anchors.right: resizeHandle.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        cursorShape: Qt.SizeAllCursor
                        drag.target: clipItem
                        onPressed: {
                            arrangeView.selectedClipId = modelData.id
                            arrangeView.controller.selectClip(modelData.id)
                        }
                        onDoubleClicked: {
                            if (arrangeView.controller.openClip(modelData.id))
                                arrangeView.openPianoRollRequested()
                        }
                        onReleased: {
                            var tracks = arrangeView.controller.tracksData
                            var beat = Math.max(0, Math.round(clipItem.x
                                                            / arrangeView.beatWidth * 4) / 4)
                            var row = Math.max(0, Math.min(tracks.length - 1,
                                                          Math.round(clipItem.y
                                                                     / arrangeView.trackHeight)))
                            arrangeView.controller.moveClip(modelData.id, beat,
                                                            tracks[row].id)
                        }
                    }

                    Rectangle {
                        id: resizeHandle
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        width: 8
                        color: resizeMouse.containsMouse ? SaudadeTheme.bgControlHover
                                                       : "transparent"
                        MouseArea {
                            id: resizeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.SizeHorCursor
                            property real pressedWidth: 0
                            property real pressedX: 0
                            onPressed: {
                                pressedWidth = clipItem.width
                                pressedX = mouse.x
                            }
                            onPositionChanged: if (pressed) {
                                clipItem.width = Math.max(arrangeView.beatWidth / 4,
                                                         pressedWidth + mouse.x - pressedX)
                            }
                            onReleased: {
                                var duration = Math.max(0.25,
                                        Math.round(clipItem.width / arrangeView.beatWidth * 4) / 4)
                                arrangeView.controller.resizeClip(modelData.id, duration)
                            }
                        }
                    }
                }
            }

            Rectangle {
                x: (arrangeView.controller ? arrangeView.controller.currentBeat : 0)
                   * arrangeView.beatWidth
                width: 1
                height: parent.height
                color: SaudadeTheme.accentPlayhead
                z: 20
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: arrangeView.visible && arrangeView.selectedClipId !== 0
        onActivated: arrangeView.selectedClipId =
                             arrangeView.controller.duplicateClip(arrangeView.selectedClipId)
    }
    Shortcut {
        sequences: ["Delete", "Backspace"]
        enabled: arrangeView.visible && arrangeView.selectedClipId !== 0
        onActivated: {
            arrangeView.controller.deleteClip(arrangeView.selectedClipId)
            arrangeView.selectedClipId = 0
        }
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: arrangeView.visible
        onActivated: arrangeView.controller.arrangementUndo()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        enabled: arrangeView.visible
        onActivated: arrangeView.controller.arrangementRedo()
    }
}
