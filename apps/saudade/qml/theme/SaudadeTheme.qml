pragma Singleton
import QtQuick

QtObject {
    id: theme

    // === Palette: Charcoal & Dark Precision Instrument ===
    readonly property color bgCanvas: "#0E0F13"
    readonly property color bgWorkspace: "#121318"
    readonly property color bgPanel: "#17191F"
    readonly property color bgPanelRaised: "#1D1F26"
    readonly property color bgControl: "#24262E"
    readonly property color bgControlHover: "#2A2D36"
    readonly property color bgControlPressed: "#30333D"

    // Lines & Separators
    readonly property color lineSoft: "#282B33"
    readonly property color lineNormal: "#353944"
    readonly property color lineFocus: "#E8E6DF"

    // Typography
    readonly property color textPrimary: "#F1F0EC"
    readonly property color textSecondary: "#B4B6BE"
    readonly property color textMuted: "#737781"
    readonly property color textGhost: "#4C5059"

    // Semantic Accents
    readonly property color accentSelection: "#8FA5BA"
    readonly property color accentPlayhead: "#D6B49A"
    readonly property color accentRecord: "#D65A5A"
    readonly property color accentWarning: "#C89A55"
    readonly property color accentSuccess: "#6F9B7A"

    // Font Families
    readonly property string fontSans: "Inter, Instrument Sans, Adwaita Sans, DejaVu Sans, sans-serif"
    readonly property string fontMono: "JetBrains Mono, DejaVu Sans Mono, monospace"

    // Typography Scale
    readonly property int textMicro: 10
    readonly property int textSmall: 11
    readonly property int textBody: 12
    readonly property int textMedium: 13
    readonly property int textTitle: 15
    readonly property int textDisplay: 18
    readonly property int textDisplayLg: 20

    // Radii
    readonly property real radiusSm: 2
    readonly property real radiusMd: 4
    readonly property real radiusLg: 6
    readonly property real radiusFull: 9999

    // Layout Metrics
    readonly property real topBarHeight: 42
    readonly property real subContextHeight: 28
    readonly property real toolRibbonHeight: 32
    readonly property real deviceRackHeight: 190
    readonly property real velocityLaneHeight: 110
    readonly property real footerHeight: 24
    readonly property real drawerWidth: 260
}
