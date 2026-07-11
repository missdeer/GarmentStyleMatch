import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string autoMatchedStyleIds: ""
    property bool busy: false

    signal confirmStyleId(string styleId)
    signal autoMatchRequested()

    onAutoMatchedStyleIdsChanged: {
        if (autoMatchedStyleIds !== "")
            styleIdField.text = autoMatchedStyleIds
    }

    implicitHeight: layout.implicitHeight + 20
    color: "#eef1f4"
    border.color: "#dee3e8"

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 8

        Button {
            Layout.fillWidth: true
            text: qsTr("自动匹配款号")
            enabled: !root.busy
            onClicked: root.autoMatchRequested()
        }

        ClearableTextField {
            id: styleIdField
            Layout.fillWidth: true
            placeholderText: qsTr("款号")
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("确认款号")
            enabled: styleIdField.text.trim().length > 0
            onClicked: root.confirmStyleId(styleIdField.text.trim())
        }
    }
}
