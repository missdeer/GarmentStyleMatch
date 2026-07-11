import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool busy: false

    signal autoMatchRequested()

    color: "#ffffff"
    border.color: "#dee3e8"

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

            ConfirmBar {
                busy: root.busy
                onAutoMatchRequested: root.autoMatchRequested()
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
            border.color: "#cfd6dd"
            color: "#ffffff"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 25
                    color: "#eef1f4"
                    border.color: "#cfd6dd"

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
                        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#cfd6dd" }
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
                        color: index % 2 === 0 ? "#ffffff" : "#f7f8fa"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0
                            Label {
                                Layout.preferredWidth: 145
                                Layout.leftMargin: 8
                                text: modelData.name
                                color: "#1c2b3a"
                                font.pixelSize: 12
                                elide: Label.ElideRight
                            }
                            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#e3e7eb" }
                            Label {
                                Layout.fillWidth: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                text: modelData.value
                                color: "#1c2b3a"
                                font.pixelSize: 12
                                elide: Label.ElideRight
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: propertyView.count === 0
                        text: emptyText
                        color: "#8fa1b0"
                    }
                }
            }
        }
    }
}
