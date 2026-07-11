import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root
    property string imagePath: ""
    property string styleId:   ""
    property var matchedItems: []
    property int    pageIndex: 0
    property int    pageCount: 0
    property bool   previousEnabled: pageIndex > 0
    property bool   nextEnabled: pageIndex + 1 < pageCount
    readonly property int previewDecodeSize: Screen.devicePixelRatio > 1.5 ? 2560 : 1920

    signal prev()
    signal next()
    signal openOriginal()
    signal matchConfirmed(string part)
    signal matchRejected(string part)

    color: "#ffffff"
    border.color: "#dee3e8"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            id: imageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Image {
                id: preview
                Layout.fillWidth: true
                Layout.fillHeight: true
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                sourceSize.width: root.previewDecodeSize
                sourceSize.height: root.previewDecodeSize
                source: root.imagePath !== "" ? "file:///" + root.imagePath : ""

                Rectangle {
                    anchors.fill: parent
                    color: "#f5f7fa"
                    visible: preview.status !== Image.Ready
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("暂无实拍图 / No image")
                        color: "#8fa1b0"
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: visible ? Math.min(220, Math.max(140, imageArea.width * 0.24)) : 0
                visible: root.matchedItems.length > 0
                color: "#f5f7fa"
                border.color: "#cfd8df"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    Repeater {
                        model: root.matchedItems

                        delegate: Rectangle {
                            id: matchCell
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredHeight: 1
                            color: "#ffffff"
                            border.color: "#dee3e8"

                            Image {
                                anchors.fill: parent
                                anchors.bottomMargin: matchLabel.height + 4
                                anchors.margins: 4
                                source: matchCell.modelData.imagePath !== "" ? "file:///" + matchCell.modelData.imagePath : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                            }

                            Row {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 5
                                spacing: 4

                                Button {
                                    width: 28
                                    height: 28
                                    text: "×"
                                    font.pixelSize: 18
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("匹配错误，删除记录")
                                    onClicked: root.matchRejected(matchCell.modelData.part)
                                }

                                Button {
                                    width: 28
                                    height: 28
                                    text: "✓"
                                    enabled: !matchCell.modelData.confirmed
                                    highlighted: matchCell.modelData.confirmed
                                    ToolTip.visible: hovered
                                    ToolTip.text: matchCell.modelData.confirmed ? qsTr("已确认") : qsTr("确认匹配")
                                    onClicked: root.matchConfirmed(matchCell.modelData.part)
                                }
                            }

                            Label {
                                id: matchLabel
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                text: qsTr("%1款号：%2").arg(matchCell.modelData.garment).arg(matchCell.modelData.styleId)
                                horizontalAlignment: Text.AlignHCenter
                                elide: Label.ElideRight
                                font.pixelSize: 11
                                font.bold: true
                                color: "#3a4a5a"
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            color: "#f5f7fa"
            border.color: "#dee3e8"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Button {
                    text: qsTr("上一张")
                    enabled: root.previousEnabled
                    onClicked: root.prev()
                }
                Button {
                    text: qsTr("下一张")
                    enabled: root.nextEnabled
                    onClicked: root.next()
                }
                Button {
                    text: qsTr("查看原图")
                    onClicked: root.openOriginal()
                }
                Label {
                    text: qsTr("第 %1 / %2 张").arg(root.pageCount === 0 ? 0 : root.pageIndex + 1).arg(root.pageCount)
                    Layout.leftMargin: 12
                    color: "#3a4a5a"
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.styleId
                    color: "#3a4a5a"
                    font.pixelSize: 12
                }
            }
        }
    }
}
