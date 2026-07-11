import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property bool busy: false

    signal autoMatchRequested()
    signal copyStyleIdsRequested(int offset, string part)
    signal copyStyleIdsToAdjacentRequested(int offset, string part)

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: qsTr("复制上一张款号"), part: "all" },
                    { label: qsTr("复制上一张上衣款号"), part: "upper" },
                    { label: qsTr("复制上一张裤裙款号"), part: "lower" }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    enabled: !root.busy
                    onClicked: root.copyStyleIdsRequested(-1, modelData.part)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: qsTr("复制下一张款号"), part: "all" },
                    { label: qsTr("复制下一张上衣款号"), part: "upper" },
                    { label: qsTr("复制下一张裤裙款号"), part: "lower" }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    enabled: !root.busy
                    onClicked: root.copyStyleIdsRequested(1, modelData.part)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: qsTr("款号复制到上一张"), part: "all" },
                    { label: qsTr("上衣款号复制到上一张"), part: "upper" },
                    { label: qsTr("裤裙款号复制到上一张"), part: "lower" }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    enabled: !root.busy
                    onClicked: root.copyStyleIdsToAdjacentRequested(-1, modelData.part)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: qsTr("款号复制到下一张"), part: "all" },
                    { label: qsTr("上衣款号复制到下一张"), part: "upper" },
                    { label: qsTr("裤裙款号复制到下一张"), part: "lower" }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    enabled: !root.busy
                    onClicked: root.copyStyleIdsToAdjacentRequested(1, modelData.part)
                }
            }
        }

    }
}
