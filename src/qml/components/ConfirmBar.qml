import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string styleId: ""

    signal confirmStyleId(string styleId)

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
            onClicked: styleIdField.text = root.styleId
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
