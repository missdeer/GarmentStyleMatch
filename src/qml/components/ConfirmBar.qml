import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property bool busy: false
    property bool previousAvailable: false
    property bool nextAvailable: false

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
            text: qsTr("自动匹配当前实拍图款号")
            highlighted: true
            enabled: !root.busy
            onClicked: root.autoMatchRequested()
        }

        ButtonGroup { id: copyPartGroup }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 6
            rowSpacing: 8

            Label {
                text: qsTr("复制范围")
                font.bold: true
                color: "#3a4a5a"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: [
                        { label: qsTr("全部"), part: "all" },
                        { label: qsTr("上衣"), part: "upper" },
                        { label: qsTr("裤裙"), part: "lower" }
                    ]
                    delegate: RadioButton {
                        required property var modelData
                        property string part: modelData.part
                        Layout.fillWidth: true
                        text: modelData.label
                        checked: modelData.part === "all"
                        ButtonGroup.group: copyPartGroup
                    }
                }
            }

            Label {
                text: qsTr("从相邻图片复制到当前")
                font.bold: true
                color: "#3a4a5a"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    Layout.fillWidth: true
                    text: qsTr("← 上一张")
                    enabled: !root.busy && root.previousAvailable
                    onClicked: root.copyStyleIdsRequested(-1, copyPartGroup.checkedButton.part)
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("下一张 →")
                    enabled: !root.busy && root.nextAvailable
                    onClicked: root.copyStyleIdsRequested(1, copyPartGroup.checkedButton.part)
                }
            }

            Label {
                text: qsTr("将当前复制到相邻图片")
                font.bold: true
                color: "#3a4a5a"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    Layout.fillWidth: true
                    text: qsTr("← 上一张")
                    enabled: !root.busy && root.previousAvailable
                    onClicked: root.copyStyleIdsToAdjacentRequested(-1, copyPartGroup.checkedButton.part)
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("下一张 →")
                    enabled: !root.busy && root.nextAvailable
                    onClicked: root.copyStyleIdsToAdjacentRequested(1, copyPartGroup.checkedButton.part)
                }
            }
        }
    }
}
