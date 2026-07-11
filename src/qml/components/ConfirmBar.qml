import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property bool busy: false

    signal autoMatchRequested()

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

    }
}
