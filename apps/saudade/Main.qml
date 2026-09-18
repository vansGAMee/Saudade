import QtQuick
import QtQuick.Controls
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
    title: "Saudade — Precision Audio Workstation"
    color: SaudadeTheme.bgCanvas

    property string activeViewMode: "PIANO ROLL" // "ARRANGEMENT", "PIANO ROLL", "MIXER", "DEVICES"
    property bool drawerCollapsed: true
    property real currentZoom: 1.0

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
        onSearchTriggered: console.log("Search triggered")
        onSettingsTriggered: console.log("Settings triggered")
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

                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 0

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

                        PianoRollItem {
                            id: pianoRoll
                            objectName: "pianoRoll"
                            width: pianoRollFlickable.contentWidth
                            height: root.contentHeightCalc
                            controller: editorController
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
