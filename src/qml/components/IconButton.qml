import QtQuick
import QtQuick.Controls

Button {
    id: root

    property string iconName: ""
    property url iconSource: ""
    property string toolTipText: ""
    property string uiStyle: ""
    readonly property bool useCompactIconPadding: {
        const style = uiStyle.toLowerCase()
        return style === "fluentwinui3" || style === "imagine"
    }
    readonly property int iconExtent: Math.max(
        1, Math.floor(Math.min(availableWidth, availableHeight)))

    Binding on leftPadding {
        when: root.useCompactIconPadding
        value: 8
    }
    Binding on rightPadding {
        when: root.useCompactIconPadding
        value: 8
    }

    display: AbstractButton.IconOnly
    icon.name: iconName
    icon.source: iconSource
    icon.width: iconExtent
    icon.height: iconExtent
    ToolTip.visible: hovered
    ToolTip.text: toolTipText
}
