import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property alias model: grid.model
    property string pptPath: ""
    property int selectedCount: 0
    property bool busy: false

    signal pptPathEdited(string path)
    signal pptSearchRequested()
    signal pageToggled(int row)
    signal extractRequested()

    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pptBox.implicitHeight + 20
            color: Theme.subtleBg
            border.color: Theme.border

            PathPickerRow {
                id: pptBox
                enabled: !root.busy
                anchors.left:  parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin:  8
                anchors.rightMargin: 8

                placeholderText: qsTr("fitting方案PPT")
                uiStyle: controller.currentUiStyle
                path:  root.pptPath
                isFile: true
                dialogTitle: qsTr("选择 fitting 方案 PPT 文件")
                nameFilters: ["PowerPoint Open XML (*.pptx)", "All files (*)"]
                pickLabel:   qsTr("选择PPT")
                searchLabel: qsTr("重新加载")
                searchIconName: "view-refresh"
                onPathPicked:      (p) => root.pptPathEdited(p)
                onSearchRequested: root.pptSearchRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            color: Theme.background

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    text: qsTr("PPT页面预览")
                    font.pixelSize: 13
                    font.bold: true
                }
                Label {
                    text: qsTr("(已选 %1 页)").arg(root.selectedCount)
                    color: Theme.textMuted2
                    Layout.fillWidth: true
                    elide: Label.ElideRight
                }
                Button {
                    text: root.busy ? qsTr("正在提取…") : qsTr("从选中页提取")
                    enabled: root.selectedCount > 0 && !root.busy
                    onClicked: {
                        if (root.model)
                            root.model.selectedPagesText = pagesEdit.text
                        root.extractRequested()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            color: Theme.background

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Label {
                    text: qsTr("选中页号:")
                    color: Theme.textSecondary
                }
                ClearableTextField {
                    id: pagesEdit
                    Layout.fillWidth: true
                    placeholderText: qsTr("如 1,3,5")
                    validator: RegularExpressionValidator {
                        regularExpression: /^[\d,\s]*$/
                    }
                    onEditingFinished: {
                        if (root.model)
                            root.model.selectedPagesText = text
                    }
                    onCleared: {
                        if (root.model)
                            root.model.selectedPagesText = ""
                    }
                }
                Binding {
                    target: pagesEdit
                    property: "text"
                    value: root.model ? root.model.selectedPagesText : ""
                    when: !pagesEdit.activeFocus
                    restoreMode: Binding.RestoreNone
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: Math.max(140, (width - 4) / 2)
            cellHeight: cellWidth * 0.85
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property int    index
                required property int    pageIndex
                required property string imagePath
                required property bool   selected

                width:  GridView.view.cellWidth
                height: GridView.view.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: Theme.surface
                    border.color: cell.selected ? Theme.accent : Theme.border
                    border.width: cell.selected ? 2 : 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.fill: parent
                                color: Theme.surfaceAlt
                                visible: pageThumb.status !== Image.Ready
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    width: parent.width - 8
                                    spacing: 2
                                    Label {
                                        text: qsTr("第 %1 页").arg(cell.pageIndex)
                                        color: Theme.textPlaceholder
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    Label {
                                        text: "status=" + pageThumb.status
                                        color: Theme.danger
                                        font.pixelSize: 10
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    Label {
                                        text: cell.imagePath
                                        color: Theme.textSecondary
                                        font.pixelSize: 9
                                        wrapMode: Text.WrapAnywhere
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            Image {
                                id: pageThumb
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                source: cell.imagePath !== "" ? "file:///" + cell.imagePath : ""
                                onStatusChanged: {
                                    if (status === Image.Error)
                                        console.log("PptPreview Image error:", source, "reason?")
                                    else if (status === Image.Ready)
                                        console.log("PptPreview Image ready:", source)
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 4
                                width: 20; height: 20
                                radius: 3
                                color: cell.selected ? Theme.accent : Theme.overlayBadge
                                border.color: Theme.accent
                                Label {
                                    anchors.centerIn: parent
                                    text: cell.selected ? "✓" : ""
                                    color: Theme.accentText
                                    font.bold: true
                                }
                            }
                        }

                        Label {
                            text: qsTr("Page %1").arg(cell.pageIndex)
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.pageToggled(cell.index)
                    }
                }
            }
        }
    }
}
