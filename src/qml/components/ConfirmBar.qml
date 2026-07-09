import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string styleId: ""

    signal confirmSelected(int galleryRow)
    signal confirmStyleId(string styleId)
    signal previousItem()
    signal generateModel()

    // 让高度跟随内容,窗口变窄时 Flow 自动换行,ConfirmBar 会长高
    implicitHeight: layout.implicitHeight + 20
    color: "#eef1f4"
    border.color: "#dee3e8"

    RowLayout {
        id: layout
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.top:   parent.top
        anchors.margins: 10
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label { text: qsTr("当前款号:"); font.pixelSize: 12 }
                Label {
                    Layout.fillWidth: true
                    text: root.styleId
                    font.pixelSize: 12
                    color: "#1c2b3a"
                    elide: Label.ElideMiddle
                }
            }

            // 用 Flow 代替 RowLayout,宽度不够时自动换行
            Flow {
                Layout.fillWidth: true
                spacing: 12

                ButtonGroup { id: modeGroup }
                RadioButton { id: modeThumb; text: qsTr("选中小图"); checked: true; ButtonGroup.group: modeGroup }
                RadioButton { id: modeStyle; text: qsTr("确认款号");                 ButtonGroup.group: modeGroup }

                TextField {
                    id: manualId
                    implicitWidth: 200
                    placeholderText: qsTr("手输款号")
                    enabled: modeStyle.checked
                }
                Button {
                    text: qsTr("确认选中小图")
                    enabled: modeThumb.checked
                    onClicked: root.confirmSelected(-1)
                }
                Button {
                    text: qsTr("确认识别")
                    enabled: modeStyle.checked
                    onClicked: root.confirmStyleId(manualId.text.length ? manualId.text : root.styleId)
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            color: "#dee3e8"
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: 6
            Button {
                Layout.fillWidth: true
                text: qsTr("上一个 (K)")
                onClicked: root.previousItem()
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("生成微调模型")
                onClicked: root.generateModel()
            }
        }
    }
}
