import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property alias model: grid.model
    property string categoryText: qsTr("全部")
    property string searchText: ""
    property string pptPath: ""

    signal searchTextEdited(string text)
    signal categoryEdited(string text)
    signal searchRequested()
    signal pptPathEdited(string path)
    signal pptSearchRequested()

    color: "#f5f7fa"
    border.color: "#dee3e8"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- 顶部:fitting 方案 PPT 路径 ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pptBox.implicitHeight + 20
            color: "#e9edf1"
            border.color: "#dee3e8"

            PathPickerRow {
                id: pptBox
                anchors.left:  parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin:  8
                anchors.rightMargin: 8

                label: qsTr("fitting方案PPT")
                path:  root.pptPath
                isFile: true
                dialogTitle: qsTr("选择 fitting 方案 PPT 文件")
                nameFilters: ["PowerPoint (*.pptx *.ppt)", "All files (*)"]
                pickLabel:   qsTr("选择PPT")
                searchLabel: qsTr("搜索PPT")
                onPathPicked:      (p) => root.pptPathEdited(p)
                onSearchRequested: root.pptSearchRequested()
            }
        }

        // ---- 中部:款号小图库搜索栏 ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            color: "#f5f7fa"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Label {
                    text: qsTr("款号小图库")
                    font.pixelSize: 13
                    font.bold: true
                }
                ComboBox {
                    id: catBox
                    model: [qsTr("全部"), qsTr("baby"), qsTr("kids"), qsTr("adult")]
                    Layout.preferredWidth: 100
                    onCurrentTextChanged: root.categoryEdited(currentText)
                }
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("输入款号或关键词")
                    text: root.searchText
                    onEditingFinished: root.searchTextEdited(text)
                }
                Button {
                    text: qsTr("搜索")
                    onClicked: root.searchRequested()
                }
            }
        }

        // ---- 底部:2 列缩略图网格 ----
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: Math.max(120, (width - 4) / 2)
            cellHeight: cellWidth * 1.15
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property int    index
                required property string styleId
                required property string imagePath
                required property string tag
                required property int    indexLabel

                width:  GridView.view.cellWidth
                height: GridView.view.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: "white"
                    border.color: "#dee3e8"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.fill: parent
                                color: "#eef1f4"
                                visible: thumb.status !== Image.Ready
                                Label {
                                    anchors.centerIn: parent
                                    text: "sketch"
                                    color: "#8fa1b0"
                                }
                            }
                            Image {
                                id: thumb
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                source: cell.imagePath !== "" ? "file:///" + cell.imagePath : ""
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: cell.indexLabel + "  " + cell.styleId
                                font.pixelSize: 11
                                elide: Label.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                color: "#e6f0ff"
                                border.color: "#a8c4f0"
                                radius: 3
                                implicitWidth: tagLabel.implicitWidth + 8
                                implicitHeight: tagLabel.implicitHeight + 4
                                Label {
                                    id: tagLabel
                                    anchors.centerIn: parent
                                    text: cell.tag
                                    font.pixelSize: 10
                                    color: "#2f5aa8"
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onDoubleClicked: root.searchTextEdited(cell.styleId)
                    }
                }
            }
        }
    }
}
