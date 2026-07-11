import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root
    property string imagePath: ""
    property string styleId:   ""
    property int    pageIndex: 0
    property int    pageCount: 0
    property bool   previousEnabled: pageIndex > 0
    property bool   nextEnabled: pageIndex + 1 < pageCount
    readonly property int previewDecodeSize: Screen.devicePixelRatio > 1.5 ? 2560 : 1920

    signal prev()
    signal next()
    signal openOriginal()

    color: "#ffffff"
    border.color: "#dee3e8"

    ColumnLayout {
        anchors.fill: parent
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
