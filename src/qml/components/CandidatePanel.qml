import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property alias outputModel: outputView.model
    property alias inputModel:  photoView.model
    property int   currentRow: -1        // 归类列表选中行
    property int   currentPhotoRow: -1   // 实拍图片选中行
    property string photoDir: ""
    property string outputDir: ""

    signal rowActivated(int row)
    signal photoRowActivated(int row)
    signal photoDirEdited(string path)
    signal photoDirSearchRequested()
    signal outputDirEdited(string path)
    signal outputDirSearchRequested()

    color: "#f5f7fa"
    border.color: "#dee3e8"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton { text: qsTr("输入") }
            TabButton { text: qsTr("输出") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // ============ 输入 tab ============
            ColumnLayout {
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: inputPickerBox.implicitHeight + 16
                    color: "#e9edf1"
                    border.color: "#dee3e8"

                    PathPickerRow {
                        id: inputPickerBox
                        anchors.left:  parent.left
                        anchors.right: parent.right
                        anchors.top:   parent.top
                        anchors.leftMargin:  8
                        anchors.rightMargin: 8
                        anchors.topMargin:   8
                        label: qsTr("实拍图片目录")
                        path:  root.photoDir
                        dialogTitle: qsTr("选择实拍图片目录")
                        onPathPicked:      (p) => root.photoDirEdited(p)
                        onSearchRequested: root.photoDirSearchRequested()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    color: "#dfe5eb"
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: qsTr("实拍图片列表 (%1)").arg(photoView.count)
                        font.pixelSize: 12
                        font.bold: true
                        color: "#1c2b3a"
                    }
                }

                ListView {
                    id: photoView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    currentIndex: root.currentPhotoRow
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: photoCell
                        required property int index
                        required property string displayLine
                        required property bool processed

                        width: photoView.width
                        height: 26
                        readonly property bool selected: index === photoView.currentIndex
                        color: selected ? "#3a6ea5"
                               : (index % 2 === 0 ? "#ffffff" : "#f0f3f6")

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Label {
                                text: photoCell.processed ? "[✓]" : "[ ]"
                                color: photoCell.selected ? "white" : "#3a6ea5"
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: photoCell.displayLine
                                color: photoCell.selected ? "white" : "#1c2b3a"
                                font.pixelSize: 12
                                elide: Label.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.photoRowActivated(photoCell.index)
                        }
                    }
                }
            }

            // ============ 输出 tab ============
            ColumnLayout {
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: outputPickerBox.implicitHeight + 16
                    color: "#e9edf1"
                    border.color: "#dee3e8"

                    PathPickerRow {
                        id: outputPickerBox
                        anchors.left:  parent.left
                        anchors.right: parent.right
                        anchors.top:   parent.top
                        anchors.leftMargin:  8
                        anchors.rightMargin: 8
                        anchors.topMargin:   8
                        label: qsTr("输出目录")
                        path:  root.outputDir
                        dialogTitle: qsTr("选择输出目录")
                        onPathPicked:      (p) => root.outputDirEdited(p)
                        onSearchRequested: root.outputDirSearchRequested()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    color: "#dfe5eb"
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: qsTr("归类列表 (%1)").arg(outputView.count)
                        font.pixelSize: 12
                        font.bold: true
                        color: "#1c2b3a"
                    }
                }

                ListView {
                    id: outputView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    currentIndex: root.currentRow
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: outCell
                        required property int index
                        required property string displayLine
                        required property bool confirmed

                        width: outputView.width
                        height: 26
                        readonly property bool selected: index === outputView.currentIndex
                        color: selected ? "#3a6ea5"
                               : (index % 2 === 0 ? "#ffffff" : "#f0f3f6")

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Label {
                                text: outCell.confirmed ? "[✓]" : "[ ]"
                                color: outCell.selected ? "white" : "#3a6ea5"
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: outCell.displayLine
                                color: outCell.selected ? "white" : "#1c2b3a"
                                font.pixelSize: 12
                                elide: Label.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.rowActivated(outCell.index)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    color: "#e9edf1"
                    border.color: "#dee3e8"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Label { text: qsTr("列表筛选"); font.pixelSize: 12 }
                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("全部"), qsTr("未确认"), qsTr("已确认")]
                        }
                    }
                }
            }
        }
    }
}
