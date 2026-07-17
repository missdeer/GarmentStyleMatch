import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property alias outputModel: outputView.model
    property alias inputModel:  photoView.model
    property int   currentRow: -1        // 归类列表选中行
    property int   currentPhotoRow: -1   // 实拍图片选中行
    property bool inputTabActive: true
    readonly property int inputItemCount: photoView.count
    property string photoDir: ""
    property string outputDir: ""
    property string inputFilterText: ""
    property string outputFilterText: ""

    signal rowActivated(int row)
    signal photoRowActivated(int row)
    signal photoDirEdited(string path)
    signal outputDirEdited(string path)
    signal inputFilterTextEdited(string text)
    signal outputFilterTextEdited(string text)
    signal inputTabActiveEdited(bool active)

    function matchStatusMarker(status) {
        return status === 2 ? "[✓]" : (status === 1 ? "[-]" : "[]")
    }

    TextMetrics {
        id: matchStatusMetrics
        font.pixelSize: 12
        text: "[✓]"
    }

    color: Theme.background
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton {
                text: qsTr("输入")
                onClicked: root.inputTabActiveEdited(true)
            }
            TabButton {
                text: qsTr("输出")
                onClicked: root.inputTabActiveEdited(false)
            }

            Binding on currentIndex {
                value: root.inputTabActive ? 0 : 1
            }
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
                    implicitHeight: inputPickerColumn.implicitHeight + 16
                    color: Theme.subtleBg
                    border.color: Theme.border

                    ColumnLayout {
                        id: inputPickerColumn
                        anchors.left:  parent.left
                        anchors.right: parent.right
                        anchors.top:   parent.top
                        anchors.leftMargin:  8
                        anchors.rightMargin: 8
                        anchors.topMargin:   8
                        spacing: 6

                        PathPickerRow {
                            Layout.fillWidth: true
                            placeholderText: qsTr("实拍图片目录")
                            uiStyle: controller.currentUiStyle
                            path: root.photoDir
                            dialogTitle: qsTr("选择实拍图片目录")
                            showSearchButton: false
                            onPathPicked: (p) => root.photoDirEdited(p)
                        }
                        ClearableTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("输入文件名或关键词")
                            text: root.inputFilterText
                            onTextChanged: root.inputFilterTextEdited(text)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    color: Theme.captionBg
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: qsTr("实拍图片列表 (%1)").arg(photoView.count)
                        font.pixelSize: 12
                        font.bold: true
                        color: Theme.text
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
                        required property int upperMatchStatus
                        required property int lowerMatchStatus

                        width: photoView.width
                        height: 26
                        readonly property bool selected: index === photoView.currentIndex
                        color: selected ? Theme.selectionBg
                               : (index % 2 === 0 ? Theme.surface : Theme.rowAlt)

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Row {
                                spacing: 0
                                anchors.verticalCenter: parent.verticalCenter

                                Repeater {
                                    model: [photoCell.upperMatchStatus, photoCell.lowerMatchStatus]

                                    Label {
                                        required property int modelData
                                        width: Math.ceil(matchStatusMetrics.advanceWidth)
                                        text: root.matchStatusMarker(modelData)
                                        color: photoCell.selected ? Theme.selectionText : Theme.listIndicator
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            Label {
                                text: photoCell.displayLine
                                color: photoCell.selected ? Theme.selectionText : Theme.text
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
                    implicitHeight: outputPickerColumn.implicitHeight + 16
                    color: Theme.subtleBg
                    border.color: Theme.border

                    ColumnLayout {
                        id: outputPickerColumn
                        anchors.left:  parent.left
                        anchors.right: parent.right
                        anchors.top:   parent.top
                        anchors.leftMargin:  8
                        anchors.rightMargin: 8
                        anchors.topMargin:   8
                        spacing: 6

                        PathPickerRow {
                            Layout.fillWidth: true
                            placeholderText: qsTr("输出目录")
                            uiStyle: controller.currentUiStyle
                            path: root.outputDir
                            dialogTitle: qsTr("选择输出目录")
                            showSearchButton: false
                            onPathPicked: (p) => root.outputDirEdited(p)
                        }
                        ClearableTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("输入款号或关键词")
                            text: root.outputFilterText
                            onTextChanged: root.outputFilterTextEdited(text)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    color: Theme.captionBg
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: qsTr("归类列表 (%1)").arg(outputView.count)
                        font.pixelSize: 12
                        font.bold: true
                        color: Theme.text
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
                        color: selected ? Theme.selectionBg
                               : (index % 2 === 0 ? Theme.surface : Theme.rowAlt)

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Label {
                                text: outCell.confirmed ? "[✓]" : "[ ]"
                                color: outCell.selected ? Theme.selectionText : Theme.listIndicator
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: outCell.displayLine
                                color: outCell.selected ? Theme.selectionText : Theme.text
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
                    color: Theme.subtleBg
                    border.color: Theme.border

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
