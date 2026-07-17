import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property alias model: grid.model
    property string categoryText: qsTr("全部")
    property string searchText: ""

    signal searchTextEdited(string text)
    signal categoryEdited(string text)

    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            color: Theme.background

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Label {
                    text: qsTr("款号小图库 (%1)").arg(root.model ? root.model.count : 0)
                    font.pixelSize: 13
                    font.bold: true
                }
                ComboBox {
                    id: catBox
                    model: [qsTr("全部"), qsTr("baby"), qsTr("kids"), qsTr("adult")]
                    Layout.preferredWidth: 100
                    onCurrentTextChanged: root.categoryEdited(currentText)
                }
                ClearableTextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("输入款号或关键词")
                    text: root.searchText
                    onTextChanged: root.searchTextEdited(text)
                }
            }
        }

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
                required property bool   selected

                width:  GridView.view.cellWidth
                height: GridView.view.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: cell.selected ? Theme.accentLightBg : Theme.surface
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
                                visible: thumb.status !== Image.Ready
                                Label {
                                    anchors.centerIn: parent
                                    text: "sketch"
                                    color: Theme.textPlaceholder
                                }
                            }
                            Image {
                                id: thumb
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                source: cell.imagePath !== "" ? "file:///" + cell.imagePath : ""
                            }

                            Rectangle {
                                visible: cell.selected
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 4
                                width: 22
                                height: 22
                                radius: 11
                                color: Theme.accent

                                Label {
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: Theme.accentText
                                    font.bold: true
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("款号：%1").arg(cell.styleId)
                                font.pixelSize: 11
                                font.bold: true
                                elide: Label.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                color: Theme.accentTagBg
                                border.color: Theme.accentTagBorder
                                radius: 3
                                implicitWidth: tagLabel.implicitWidth + 8
                                implicitHeight: tagLabel.implicitHeight + 4
                                Label {
                                    id: tagLabel
                                    anchors.centerIn: parent
                                    text: cell.tag
                                    font.pixelSize: 10
                                    color: Theme.accent
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.model)
                                root.model.toggleSelected(cell.index)
                        }
                        onDoubleClicked: root.searchTextEdited(cell.styleId)
                    }
                }
            }
        }
    }
}
