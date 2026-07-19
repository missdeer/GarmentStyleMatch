import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property alias model: grid.model
    property string categoryText: qsTr("全部")
    property string searchText: ""
    property string uiStyle: ""
    property bool currentPhotoSelected: false

    signal searchTextEdited(string text)
    signal categoryEdited(string text)
    signal matchRequested(int galleryRow, string part)
    signal confirmRequested(int galleryRow, string part)

    color: Theme.background

    function urlToLocalPath(u) {
        var s = u.toString().replace(/^file:\/\//, "")
        if (/^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        return s
    }

    Menu {
        id: matchMenu
        property int galleryRow: -1

        MenuItem {
            text: qsTr("匹配为上衣")
            onTriggered: root.matchRequested(matchMenu.galleryRow, "upper")
        }
        MenuItem {
            text: qsTr("匹配为裤裙")
            onTriggered: root.matchRequested(matchMenu.galleryRow, "lower")
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("确认为上衣")
            onTriggered: root.confirmRequested(matchMenu.galleryRow, "upper")
        }
        MenuItem {
            text: qsTr("确认为裤裙")
            onTriggered: root.confirmRequested(matchMenu.galleryRow, "lower")
        }
    }

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

                ClearableTextField {
                    id: pathField
                    Layout.fillWidth: true
                    placeholderText: qsTr("从文件夹或压缩文件中加载款号小图库")
                }

                IconButton {
                    id: loadFromFolderButton
                    Layout.preferredWidth: pathField.implicitHeight
                    Layout.preferredHeight: pathField.implicitHeight
                    iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/folder-open.svg"
                    toolTipText: qsTr("从文件夹加载款号小图库")
                    uiStyle: root.uiStyle
                    onClicked: folderDlg.open()
                }

                IconButton {
                    id: loadFromArchiveButton
                    Layout.preferredWidth: pathField.implicitHeight
                    Layout.preferredHeight: pathField.implicitHeight
                    iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/archive.svg"
                    toolTipText: qsTr("从压缩文件加载款号小图库")
                    uiStyle: root.uiStyle
                    onClicked: archiveDlg.open()
                }
            }

            FolderDialog {
                id: folderDlg
                title: qsTr("选择款号小图库文件夹")
                onAccepted: {
                    pathField.text = root.urlToLocalPath(selectedFolder)
                }
            }

            FileDialog {
                id: archiveDlg
                title: qsTr("选择款号小图库压缩文件")
                nameFilters: [
                    qsTr("压缩文件 (*.zip *.7z *.rar *.tar *.tar.gz *.tgz *.tar.bz2 *.tbz2 *.tar.xz *.txz)"),
                    qsTr("所有文件 (*)")
                ]
                onAccepted: {
                    pathField.text = root.urlToLocalPath(selectedFile)
                }
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
                spacing: 6

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

                width:  GridView.view.cellWidth
                height: GridView.view.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: Theme.surface
                    border.color: Theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Item {
                            id: thumbArea
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

                            HoverHandler {
                                id: thumbHover
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor
                                z: 1
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton && root.currentPhotoSelected) {
                                        matchMenu.galleryRow = cell.index
                                        matchMenu.popup()
                                    }
                                }
                                onDoubleClicked: function(mouse) {
                                    if (mouse.button === Qt.LeftButton)
                                        root.searchTextEdited(cell.styleId)
                                }
                            }

                            Grid {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 4
                                columns: 2
                                spacing: 4
                                visible: thumbHover.hovered
                                z: 2

                                Button {
                                    id: matchUpperButton
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-match-upper.svg"
                                            color: matchUpperButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式匹配为当前实拍图的上衣（待确认）")
                                    onClicked: root.matchRequested(cell.index, "upper")
                                }

                                Button {
                                    id: confirmUpperButton
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-confirm-upper.svg"
                                            color: confirmUpperButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式确认为当前实拍图的上衣")
                                    onClicked: root.confirmRequested(cell.index, "upper")
                                }

                                Button {
                                    id: matchLowerButton
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-match-lower.svg"
                                            color: matchLowerButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式匹配为当前实拍图的裤裙（待确认）")
                                    onClicked: root.matchRequested(cell.index, "lower")
                                }

                                Button {
                                    id: confirmLowerButton
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-confirm-lower.svg"
                                            color: confirmLowerButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式确认为当前实拍图的裤裙")
                                    onClicked: root.confirmRequested(cell.index, "lower")
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

                }
            }
        }
    }
}
