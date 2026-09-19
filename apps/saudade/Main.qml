import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import saudade.ui 1.0

import "qml/theme"
import "qml/components"
import "qml/layout"

ApplicationWindow {
    id: root
    visible: true
    width: 1440
    height: 960
    minimumWidth: 960
    minimumHeight: 640
    title: (editorController.projectDirty ? "* " : "")
           + (editorController.projectPath.length > 0
              ? editorController.projectPath.split("/").pop() : "Untitled")
           + " — Saudade"
    color: SaudadeTheme.bgCanvas

    property string activeViewMode: "PIANO ROLL" // "ARRANGEMENT", "PIANO ROLL", "MIXER", "DEVICES"
    property bool drawerCollapsed: true
    property real currentZoom: 1.0
    property string pendingProjectAction: ""
    property bool forceClosing: false

    function textEditorOwnsFocus() {
        return activeFocusItem
                && (activeFocusItem instanceof TextInput
                    || activeFocusItem instanceof TextEdit)
    }

    function requestProjectAction(action) {
        if (editorController.projectDirty) {
            pendingProjectAction = action
            unsavedDialog.open()
        } else {
            performProjectAction(action)
        }
    }

    function performProjectAction(action) {
        if (action === "new") {
            editorController.newProject()
        } else if (action === "open") {
            openProjectDialog.open()
        } else if (action === "close") {
            forceClosing = true
            root.close()
        }
        pendingProjectAction = ""
    }

    function saveThenContinue() {
        if (editorController.projectPath.length === 0) {
            saveProjectDialog.open()
        } else if (editorController.saveProject()) {
            performProjectAction(pendingProjectAction)
        }
    }

    onClosing: function(close) {
        if (!forceClosing && editorController.projectDirty) {
            close.accepted = false
            pendingProjectAction = "close"
            unsavedDialog.open()
        }
    }

    Component.onCompleted: {
        if (editorController.recoveryAvailable)
            recoveryDialog.open()
    }

    Dialog {
        id: recoveryDialog
        modal: true
        anchors.centerIn: parent
        width: 440
        title: "Recover unsaved work?"
        closePolicy: Popup.NoAutoClose
        contentItem: Column {
            spacing: 14
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Saudade found an autosave from a session that did not finish cleanly."
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 13
                color: SaudadeTheme.textPrimary
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                SaudadeButton {
                    text: "Discard"
                    onClicked: {
                        editorController.discardRecovery()
                        recoveryDialog.close()
                    }
                }
                SaudadeButton {
                    text: "Recover"
                    variant: "primary"
                    onClicked: {
                        editorController.recoverAutosave()
                        recoveryDialog.close()
                    }
                }
            }
        }
    }

    FileDialog {
        id: openProjectDialog
        title: "Open Saudade Project"
        nameFilters: ["Saudade Project (*.dawproj)"]
        fileMode: FileDialog.OpenFile
        onAccepted: editorController.openProject(selectedFile)
    }

    FileDialog {
        id: saveProjectDialog
        title: "Save Saudade Project"
        nameFilters: ["Saudade Project (*.dawproj)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "dawproj"
        onAccepted: {
            if (editorController.saveProjectAs(selectedFile)
                    && root.pendingProjectAction.length > 0)
                root.performProjectAction(root.pendingProjectAction)
        }
    }

    FileDialog {
        id: importMidiDialog
        title: "Import MIDI"
        nameFilters: ["MIDI Files (*.mid *.midi)"]
        fileMode: FileDialog.OpenFile
        onAccepted: editorController.importMidi(
                            selectedFile,
                            Math.round(editorController.currentBeat * 4) / 4)
    }

    FileDialog {
        id: exportWavDialog
        title: "Export Stereo WAV"
        nameFilters: ["Wave Audio (*.wav)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        onAccepted: editorController.exportWav(selectedFile)
    }

    Dialog {
        id: exportProgressDialog
        modal: true
        anchors.centerIn: parent
        width: 420
        title: "Rendering audio"
        visible: editorController.exportInProgress
        closePolicy: Popup.NoAutoClose
        contentItem: Column {
            spacing: 12
            ProgressBar {
                width: parent.width
                from: 0
                to: 1
                value: editorController.exportProgress
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(editorController.exportProgress * 100) + "%"
                    font.family: SaudadeTheme.fontMono
                    font.pixelSize: 11
                    color: SaudadeTheme.textSecondary
                }
                SaudadeButton {
                    text: "Cancel"
                    onClicked: editorController.cancelExport()
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        z: 1000
        onDropped: function(drop) {
            if (!drop.hasUrls || drop.urls.length === 0) return
            var source = drop.urls[0].toString()
            var lower = source.toLowerCase()
            if (lower.endsWith(".mid") || lower.endsWith(".midi")) {
                editorController.importMidi(
                            source,
                            Math.round(editorController.currentBeat * 4) / 4)
                drop.accept()
            }
        }
    }

    Dialog {
        id: unsavedDialog
        modal: true
        anchors.centerIn: parent
        width: 420
        title: "Save changes?"
        closePolicy: Popup.NoAutoClose

        contentItem: Column {
            spacing: 14
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "This project has changes that have not been saved."
                font.family: SaudadeTheme.fontSans
                font.pixelSize: 13
                color: SaudadeTheme.textPrimary
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                SaudadeButton {
                    text: "Cancel"
                    onClicked: {
                        root.pendingProjectAction = ""
                        unsavedDialog.close()
                    }
                }
                SaudadeButton {
                    text: "Discard"
                    onClicked: {
                        unsavedDialog.close()
                        root.performProjectAction(root.pendingProjectAction)
                    }
                }
                SaudadeButton {
                    text: "Save"
                    variant: "primary"
                    onClicked: {
                        unsavedDialog.close()
                        root.saveThenContinue()
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.New]
        enabled: !root.textEditorOwnsFocus()
        onActivated: root.requestProjectAction("new")
    }
    Shortcut {
        sequences: [StandardKey.Open]
        enabled: !root.textEditorOwnsFocus()
        onActivated: root.requestProjectAction("open")
    }
    Shortcut {
        sequences: [StandardKey.Save]
        enabled: !root.textEditorOwnsFocus()
        onActivated: {
            if (editorController.projectPath.length === 0)
                saveProjectDialog.open()
            else
                editorController.saveProject()
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: !root.textEditorOwnsFocus()
        onActivated: saveProjectDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+I"
        enabled: !root.textEditorOwnsFocus()
        onActivated: importMidiDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+E"
        enabled: !root.textEditorOwnsFocus() && !editorController.exportInProgress
        onActivated: exportWavDialog.open()
    }
    Shortcut {
        sequence: "Space"
        enabled: !root.textEditorOwnsFocus()
        onActivated: {
            if (editorController.isPlaying) editorController.stop()
            else editorController.play()
        }
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.undo()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.redo()
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.selectAll()
    }
    Shortcut {
        sequences: [StandardKey.Copy]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.copy()
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.paste()
    }
    Shortcut {
        sequence: "Ctrl+D"
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.duplicate()
    }
    Shortcut {
        sequences: ["Delete", "Backspace"]
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: editorController.deleteSelected()
    }
    Shortcut {
        sequence: "B"
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: toolRibbon.activeTool = "pencil"
    }
    Shortcut {
        sequence: "V"
        enabled: root.activeViewMode === "PIANO ROLL" && !root.textEditorOwnsFocus()
        onActivated: toolRibbon.activeTool = "select"
    }

    // Coordinate & Pitch configuration for Piano Roll
    readonly property int minPitch: 48 // C3
    readonly property int maxPitch: 84 // C6
    readonly property real rowHeight: 20.0
    readonly property real beatWidth: 180.0 * currentZoom
    readonly property real contentHeightCalc: (maxPitch - minPitch + 1) * rowHeight

    // ==========================================
    // 1. Top Transport Bar (42px)
    // ==========================================
    TopTransportBar {
        id: topTransport
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        controller: editorController
        activeView: root.activeViewMode
    }

    // ==========================================
    // 2. Sub-Context / View Switcher Bar (28px)
    // ==========================================
    SubContextBar {
        id: subContextBar
        anchors.top: topTransport.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        activeView: root.activeViewMode
        controller: editorController
        onViewSelected: (viewName) => {
            root.activeViewMode = viewName;
        }
    }

    // ==========================================
    // 3. Secondary Tool Ribbon (32px - Piano Roll)
    // ==========================================
    ToolRibbon {
        id: toolRibbon
        anchors.top: subContextBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: root.activeViewMode === "PIANO ROLL"
        height: visible ? SaudadeTheme.toolRibbonHeight : 0
        controller: editorController
        zoomLevel: root.currentZoom
        onZoomInRequested: root.currentZoom = Math.min(2.0, root.currentZoom + 0.25)
        onZoomOutRequested: root.currentZoom = Math.max(0.5, root.currentZoom - 0.25)
    }

    // ==========================================
    // 4. Global Status Footer (24px)
    // ==========================================
    FooterStatusBar {
        id: footerStatus
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        controller: editorController
    }

    // ==========================================
    // 5. Bottom Docked Device Rack (Collapsible)
    // ==========================================
    BottomDeviceRack {
        id: bottomRack
        anchors.bottom: footerStatus.top
        anchors.left: parent.left
        anchors.right: parent.right
        collapsed: true
        controller: editorController
        visible: root.activeViewMode !== "DEVICES"
        height: visible ? (collapsed ? 28 : SaudadeTheme.deviceRackHeight) : 0
    }

    // ==========================================
    // 6. Central Workstation Body Area
    // ==========================================
    Item {
        id: centerWorkArea
        anchors.top: toolRibbon.bottom
        anchors.bottom: bottomRack.visible ? bottomRack.top : footerStatus.top
        anchors.left: parent.left
        anchors.right: parent.right

        // Left Browser Drawer (Asset Matrix)
        LeftBrowserDrawer {
            id: leftDrawer
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            collapsed: root.drawerCollapsed
            onCloseRequested: root.drawerCollapsed = true
            onOpenProjectRequested: root.requestProjectAction("open")
            onImportMidiRequested: importMidiDialog.open()
            onExportWavRequested: exportWavDialog.open()
        }

        // Center Views Area
        Item {
            id: mainViewContainer
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: leftDrawer.right
            anchors.right: parent.right
            clip: true

            // ==========================================
            // VIEW A: PIANO ROLL COMPOSER VIEW
            // ==========================================
            Item {
                id: pianoRollView
                anchors.fill: parent
                visible: root.activeViewMode === "PIANO ROLL"

                // Timeline Ruler Bar (Top of Piano Roll)
                Rectangle {
                    id: rulerBar
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 24
                    color: SaudadeTheme.bgCanvas
                    border.width: 0

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: SaudadeTheme.lineNormal
                    }

                    Row {
                        anchors.fill: parent

                        // Left keys spacer
                        Rectangle {
                            width: keysContainer.width
                            height: parent.height
                            color: SaudadeTheme.bgCanvas
                            border.width: 0

                            Rectangle {
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                width: 1
                                color: SaudadeTheme.lineNormal
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "KEYS"
                                font.family: SaudadeTheme.fontMono
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                color: SaudadeTheme.textMuted
                            }
                        }

                        // Timeline Bar Numbers
                        Item {
                            width: parent.width - keysContainer.width
                            height: parent.height
                            clip: true

                            Item {
                                x: -pianoRollFlickable.contentX
                                width: pianoRollFlickable.contentWidth
                                height: parent.height

                                MouseArea {
                                    anchors.fill: parent
                                    z: 1
                                    onClicked: (mouse) => {
                                        var clickedBeat = mouse.x / root.beatWidth;
                                        if (editorController) {
                                            editorController.seekBeats(Math.max(0.0, clickedBeat));
                                        }
                                    }
                                }

                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 0
                                    z: 2

                                    Repeater {
                                        model: Math.ceil((editorController ? editorController.patternLength : 4.0) * 2)
                                        Item {
                                            width: root.beatWidth * 4
                                            height: 24

                                            Text {
                                                anchors.left: parent.left
                                                anchors.leftMargin: 6
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: (index + 1) + ".1"
                                                font.family: SaudadeTheme.fontMono
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: SaudadeTheme.textPrimary
                                            }

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.bottom: parent.bottom
                                                width: 1
                                                height: 6
                                                color: SaudadeTheme.lineNormal
                                            }
                                        }
                                    }
                                }

                                // Draggable Loop Region Overlay
                                Item {
                                    id: loopRegion
                                    visible: editorController ? editorController.loopEnabled : false
                                    x: (editorController ? editorController.loopStartBeat : 0.0) * root.beatWidth
                                    width: Math.max(8, ((editorController ? editorController.loopEndBeat : 16.0) - (editorController ? editorController.loopStartBeat : 0.0)) * root.beatWidth)
                                    height: parent.height
                                    z: 10

                                    Rectangle {
                                        anchors.fill: parent
                                        color: "#D6B49A"
                                        opacity: 0.15
                                    }

                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 3
                                        color: "#D6B49A"
                                    }

                                    // Left handle (Start)
                                    Rectangle {
                                        id: loopLeftHandle
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 6
                                        color: loopLeftMouse.containsMouse || loopLeftMouse.pressed ? "#F1EEE7" : "#D6B49A"

                                        MouseArea {
                                            id: loopLeftMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.SizeHorCursor

                                            property real pressX: 0
                                            property real initialStart: 0

                                            onPressed: (mouse) => {
                                                pressX = mouse.x;
                                                initialStart = editorController.loopStartBeat;
                                            }
                                            onPositionChanged: (mouse) => {
                                                if (pressed && editorController) {
                                                    var dBeats = (mouse.x - pressX) / root.beatWidth;
                                                    var newStart = Math.max(0.0, Math.min(editorController.loopEndBeat - 0.25, initialStart + dBeats));
                                                    editorController.setLoopStartBeat(newStart);
                                                }
                                            }
                                        }
                                    }

                                    // Right handle (End)
                                    Rectangle {
                                        id: loopRightHandle
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 6
                                        color: loopRightMouse.containsMouse || loopRightMouse.pressed ? "#F1EEE7" : "#D6B49A"

                                        MouseArea {
                                            id: loopRightMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.SizeHorCursor

                                            property real pressX: 0
                                            property real initialEnd: 0

                                            onPressed: (mouse) => {
                                                pressX = mouse.x;
                                                initialEnd = editorController.loopEndBeat;
                                            }
                                            onPositionChanged: (mouse) => {
                                                if (pressed && editorController) {
                                                    var dBeats = (mouse.x - pressX) / root.beatWidth;
                                                    var newEnd = Math.max(editorController.loopStartBeat + 0.25, initialEnd + dBeats);
                                                    editorController.setLoopEndBeat(newEnd);
                                                }
                                            }
                                        }
                                    }

                                    // Drag middle body
                                    MouseArea {
                                        anchors.left: loopLeftHandle.right
                                        anchors.right: loopRightHandle.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        hoverEnabled: true
                                        cursorShape: Qt.SizeAllCursor

                                        property real pressX: 0
                                        property real initialStart: 0
                                        property real initialEnd: 0

                                        onPressed: (mouse) => {
                                            pressX = mouse.x;
                                            initialStart = editorController.loopStartBeat;
                                            initialEnd = editorController.loopEndBeat;
                                        }
                                        onPositionChanged: (mouse) => {
                                            if (pressed && editorController) {
                                                var dBeats = (mouse.x - pressX) / root.beatWidth;
                                                var len = initialEnd - initialStart;
                                                var newStart = Math.max(0.0, initialStart + dBeats);
                                                var newEnd = newStart + len;
                                                editorController.setLoopRange(newStart, newEnd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Bottom Docked Velocity & Expression Lane Shelf
                VelocityLane {
                    id: velocityLaneShelf
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    controller: editorController
                    beatWidth: root.beatWidth
                    contentXOffset: pianoRollFlickable.contentX
                    keybedWidth: keysContainer.width
                }

                // Central Interactive Matrix (Keys + Note Grid)
                Item {
                    id: matrixArea
                    anchors.top: rulerBar.bottom
                    anchors.bottom: velocityLaneShelf.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    clip: true

                    // Pinned Piano Keys (Left Column)
                    Item {
                        id: keysContainer
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 56
                        clip: true
                        z: 10

                        PianoKeysItem {
                            id: pianoKeys
                            x: 0
                            y: -pianoRollFlickable.contentY
                            width: keysContainer.width
                            height: root.contentHeightCalc
                            controller: editorController
                            rowHeight: root.rowHeight
                            minPitch: root.minPitch
                            maxPitch: root.maxPitch
                        }

                        // Octave labels overlay (C3, C4, C5, C6)
                        Item {
                            id: keysLabels
                            x: 0
                            y: -pianoRollFlickable.contentY
                            width: keysContainer.width
                            height: root.contentHeightCalc

                            Repeater {
                                model: root.maxPitch - root.minPitch + 1
                                delegate: Item {
                                    readonly property int pitch: root.maxPitch - index
                                    readonly property bool isC: (pitch % 12) === 0
                                    readonly property int octave: Math.floor(pitch / 12) - 1

                                    visible: isC
                                    x: 0
                                    y: index * root.rowHeight
                                    width: keysContainer.width
                                    height: root.rowHeight

                                    Text {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "C" + parent.octave
                                        font.family: SaudadeTheme.fontMono
                                        font.pixelSize: 9
                                        font.weight: Font.DemiBold
                                        color: SaudadeTheme.textPrimary
                                    }
                                }
                            }
                        }

                        // Right border line for keys
                        Rectangle {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            width: 1
                            color: SaudadeTheme.lineNormal
                        }
                    }

                    // Piano Roll Canvas Scrollable View
                    Flickable {
                        id: pianoRollFlickable
                        anchors.left: keysContainer.right
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        contentWidth: Math.max(width, editorController.patternLength * root.beatWidth)
                        contentHeight: root.contentHeightCalc

                        WheelHandler {
                            acceptedModifiers: Qt.ControlModifier
                            onWheel: function(event) {
                                var cursorBeat = (pianoRollFlickable.contentX
                                                  + event.position.x) / root.beatWidth
                                var nextZoom = Math.max(0.35, Math.min(3.0,
                                        root.currentZoom
                                        * (event.angleDelta.y > 0 ? 1.12 : 0.89)))
                                root.currentZoom = nextZoom
                                pianoRollFlickable.contentX = Math.max(0,
                                        cursorBeat * root.beatWidth - event.position.x)
                                event.accepted = true
                            }
                        }

                        PianoRollItem {
                            id: pianoRoll
                            objectName: "pianoRoll"
                            width: pianoRollFlickable.contentWidth
                            height: root.contentHeightCalc
                            controller: editorController
                            activeTool: toolRibbon.activeTool
                            rowHeight: root.rowHeight
                            beatWidth: root.beatWidth
                            minPitch: root.minPitch
                            maxPitch: root.maxPitch
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AlwaysOn
                            contentItem: Rectangle {
                                implicitWidth: 4
                                radius: 2
                                color: SaudadeTheme.bgControlHover
                            }
                        }
                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitHeight: 4
                                radius: 2
                                color: SaudadeTheme.bgControlHover
                            }
                        }
                    }
                }
            }

            // ==========================================
            // VIEW B: ARRANGEMENT WORKSPACE VIEW
            // ==========================================
            ArrangementView {
                id: arrangementView
                anchors.fill: parent
                visible: root.activeViewMode === "ARRANGEMENT"
                controller: editorController
                onOpenPianoRollRequested: {
                    root.activeViewMode = "PIANO ROLL";
                }
                onImportMidiRequested: importMidiDialog.open()
                onExportWavRequested: exportWavDialog.open()
            }

            // ==========================================
            // VIEW C: MIXER CONSOLE VIEW
            // ==========================================
            MixerConsoleView {
                id: mixerConsoleView
                anchors.fill: parent
                visible: root.activeViewMode === "MIXER"
                controller: editorController
            }

            // ==========================================
            // VIEW D: DEVICES VIEW
            // ==========================================
            Item {
                id: devicesFullView
                anchors.fill: parent
                visible: root.activeViewMode === "DEVICES"

                Rectangle {
                    anchors.fill: parent
                    color: SaudadeTheme.bgWorkspace

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "FULL INSTRUMENT & DEVICE MATRIX"
                            font.family: SaudadeTheme.fontMono
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: SaudadeTheme.textPrimary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Native PolySynth Node • Gain Node (-12dB) • Master Output"
                            font.family: SaudadeTheme.fontSans
                            font.pixelSize: 11
                            color: SaudadeTheme.textSecondary
                        }
                        SaudadeButton {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Return to Piano Roll"
                            variant: "primary"
                            onClicked: root.activeViewMode = "PIANO ROLL"
                        }
                    }
                }
            }
        }
    }
}
