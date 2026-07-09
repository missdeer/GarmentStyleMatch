import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string titleText: ""
    property string subtitleText: ""

    implicitHeight: 56
    color: "#1f2c3a"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Label {
            text: root.titleText
            color: "white"
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            text: root.subtitleText
            color: "#c0d0dc"
            font.pixelSize: 13
        }

        Item { Layout.fillWidth: true }

        Label {
            text: qsTr("有参考目录分款 / 新品无参考候选 / 继续上次确认")
            color: "#8fa1b0"
            font.pixelSize: 12
        }
    }
}
