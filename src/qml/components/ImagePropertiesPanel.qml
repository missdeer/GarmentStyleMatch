import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property bool busy: false
    property bool batchAutoMatchInProgress: false
    property bool previousPhotoAvailable: false
    property bool nextPhotoAvailable: false

    signal autoMatchRequested()
    signal autoMatchAllRequested()
    signal autoMatchAllUnconfirmedRequested()
    signal cancelAutoMatchAllRequested()
    signal copyStyleIdsRequested(int offset, string part)
    signal copyStyleIdsToAdjacentRequested(int offset, string part)

    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: categoryTabs
            Layout.fillWidth: true
            TabButton { text: qsTr("操作") }
            TabButton { text: qsTr("文件信息") }
            TabButton { text: qsTr("图像信息") }
            TabButton { text: qsTr("IPTC") }
            TabButton { text: qsTr("EXIF") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: categoryTabs.currentIndex

            MatchPanel {
                busy: root.busy
                batchAutoMatchInProgress: root.batchAutoMatchInProgress
                previousAvailable: root.previousPhotoAvailable
                nextAvailable: root.nextPhotoAvailable
                onAutoMatchRequested: root.autoMatchRequested()
                onAutoMatchAllRequested: root.autoMatchAllRequested()
                onAutoMatchAllUnconfirmedRequested: root.autoMatchAllUnconfirmedRequested()
                onCancelAutoMatchAllRequested: root.cancelAutoMatchAllRequested()
                onCopyStyleIdsRequested: (offset, part) => root.copyStyleIdsRequested(offset, part)
                onCopyStyleIdsToAdjacentRequested: (offset, part) => root.copyStyleIdsToAdjacentRequested(offset, part)
            }
            PropertyList { entries: imageMetadata.fileInfo }
            PropertyList { entries: imageMetadata.imageInfo }
            PropertyList { entries: imageMetadata.iptc; emptyText: qsTr("当前图片不包含 IPTC 信息") }
            PropertyList { entries: imageMetadata.exif; emptyText: qsTr("当前图片不包含 EXIF 信息") }
        }
    }

    component PropertyList: Item {
        property var entries: []
        property string emptyText: qsTr("请选择一张实拍图")

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            border.color: Theme.borderSoft
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 25
                    color: Theme.surfaceAlt
                    border.color: Theme.borderSoft

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0
                        Label {
                            Layout.preferredWidth: 145
                            Layout.leftMargin: 8
                            text: qsTr("名称")
                            font.bold: true
                            font.pixelSize: 12
                        }
                        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.borderSoft }
                        Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            text: qsTr("值")
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                }

                ListView {
                    id: propertyView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: entries
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: propertyView.width
                        height: 25
                        color: index % 2 === 0 ? Theme.surface : Theme.rowAlt2

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0
                            Label {
                                Layout.preferredWidth: 145
                                Layout.leftMargin: 8
                                text: modelData.name
                                color: Theme.text
                                font.pixelSize: 12
                                elide: Label.ElideRight
                            }
                            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.divider }
                            Label {
                                Layout.fillWidth: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                text: modelData.value
                                color: Theme.text
                                font.pixelSize: 12
                                elide: Label.ElideRight
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: propertyView.count === 0
                        text: emptyText
                        color: Theme.textPlaceholder
                    }
                }
            }
        }
    }
}
