pragma Singleton
import QtQuick

QtObject {
    id: theme

    // Qt.Dark == Qt::ColorScheme::Dark (2). Qt::ColorScheme::Unknown falls back to light.
    readonly property bool dark: Application.styleHints.colorScheme === Qt.Dark

    // Panel surfaces
    readonly property color background:  dark ? "#1e2126" : "#f5f7fa"
    readonly property color surface:     dark ? "#2a2e35" : "#ffffff"
    readonly property color surfaceAlt:  dark ? "#24272d" : "#eef1f4"
    readonly property color surfaceAlt2: dark ? "#24272d" : "#eef3f6"
    readonly property color subtleBg:    dark ? "#262a31" : "#e9edf1"
    readonly property color captionBg:   dark ? "#2b3038" : "#dfe5eb"
    readonly property color rowAlt:      dark ? "#262a31" : "#f0f3f6"
    readonly property color rowAlt2:     dark ? "#262a31" : "#f7f8fa"

    // Borders / dividers
    readonly property color border:       dark ? "#3a4048" : "#dee3e8"
    readonly property color borderStrong: dark ? "#454b53" : "#cfd8df"
    readonly property color borderSoft:   dark ? "#3a4048" : "#cfd6dd"
    readonly property color divider:      dark ? "#363b42" : "#e3e7eb"
    readonly property color thumbBorder:  dark ? "#566068" : "#aeb9c2"

    // Text
    readonly property color text:            dark ? "#e6ecf2" : "#1c2b3a"
    readonly property color textSecondary:   dark ? "#c2ccd6" : "#3a4a5a"
    readonly property color textMuted:       dark ? "#8f99a3" : "#5f6b76"
    readonly property color textMuted2:      dark ? "#98a2ac" : "#6b7a89"
    readonly property color textPlaceholder: dark ? "#7f8a94" : "#8fa1b0"
    readonly property color textDisabled:    dark ? "#6d7680" : "#9aa7b2"

    // Selection / accent
    readonly property color selectionBg:       dark ? "#3f74a8" : "#3a6ea5"
    readonly property color selectionText:     "white"
    readonly property color listIndicator:     dark ? "#7fb0e6" : "#3a6ea5"
    readonly property color accent:            dark ? "#4a86d1" : "#2f5aa8"
    readonly property color accentText:        "white"
    readonly property color accentLightBg:     dark ? "#2a344a" : "#f3f7ff"
    readonly property color accentTagBg:       dark ? "#2a3547" : "#e6f0ff"
    readonly property color accentTagBorder:   dark ? "#395a8a" : "#a8c4f0"
    readonly property color accentThumbBorder: dark ? "#5a97d3" : "#2f73b7"
    readonly property color overlayBadge:      dark ? "#2a2e35cc" : "#ffffffcc"

    // Header bar
    readonly property color headerBg:        dark ? "#12181f" : "#1f2c3a"
    readonly property color headerText:      "white"
    readonly property color headerLink:      dark ? "#a8caea" : "#b9d8f2"
    readonly property color headerLinkHover: "#ffffff"

    // Status bar
    readonly property color statusBg:     dark ? "#262a31" : "#e9edf1"
    readonly property color statusBorder: dark ? "#3a4048" : "#dee3e8"
    readonly property color statusText:   dark ? "#c2ccd6" : "#3a4a5a"
    readonly property color statusBusy:   dark ? "#7aa8e6" : "#2f5aa8"
    readonly property color statusIdle:   dark ? "#8b95a0" : "#6b7a89"

    // Splitter handle
    readonly property color splitterIdle:    dark ? "#3a4048" : "#d8e0e7"
    readonly property color splitterHover:   dark ? "#4c5762" : "#b9cbdc"
    readonly property color splitterPressed: dark ? "#6a7d90" : "#8aa9c7"

    // Match status badges (garment upper/lower)
    readonly property color statusOkBg:       dark ? "#2a4a30" : "#dff2e1"
    readonly property color statusOkBorder:   dark ? "#4a8f56" : "#68a871"
    readonly property color statusWarnBg:     dark ? "#4a3f1d" : "#fff0c2"
    readonly property color statusWarnBorder: dark ? "#ad8730" : "#c89a32"
    readonly property color statusNoneBg:     dark ? "#2f353c" : "#e3e8ed"
    readonly property color statusNoneBorder: dark ? "#556069" : "#9eabb6"

    // Warning / danger
    readonly property color danger: dark ? "#e07070" : "#c04a4a"

    // Clear button (ClearableTextField)
    readonly property color clearIdle:  dark ? "#6b7580" : "#b0bcc7"
    readonly property color clearHover: dark ? "#8a95a0" : "#8a99a8"
    readonly property color clearText:  "white"
}
