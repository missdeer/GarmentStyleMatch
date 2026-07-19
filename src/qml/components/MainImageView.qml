import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import GarmentStyleMatch

Rectangle {
    id: root
    property string imagePath: ""
    property string styleId:   ""
    property var matchedItems: []
    property bool inputTabActive: false
    property string uiStyle: ""
    property bool showAdjacentPhotoPreviews: false
    property string previousPhotoPath: ""
    property string nextPhotoPath: ""
    property int previousPhotoUpperMatchStatus: 0
    property int previousPhotoLowerMatchStatus: 0
    property int nextPhotoUpperMatchStatus: 0
    property int nextPhotoLowerMatchStatus: 0
    property int    pageIndex: 0
    property int    pageCount: 0
    property bool   previousEnabled: pageIndex > 0
    property bool   nextEnabled: pageIndex + 1 < pageCount
    readonly property int previewDecodeSize: Screen.devicePixelRatio > 1.5 ? 2560 : 1920

    signal prev()
    signal next()
    signal previousUnmatchedPhoto()
    signal nextUnmatchedPhoto()
    signal previousUnconfirmedPhoto()
    signal nextUnconfirmedPhoto()
    signal openOriginal()
    signal matchConfirmed(string part)
    signal matchRejected(string part)

    function matchStatusMarker(status) {
        return status === 2 ? "✓" : (status === 1 ? "-" : " ")
    }

    function matchStatusToolTip(garment, status) {
        const state = status === 2 ? qsTr("已确认") : (status === 1 ? qsTr("已匹配未确认") : qsTr("未匹配"))
        return qsTr("%1：%2").arg(garment).arg(state)
    }

    function matchStatusBackground(status) {
        return status === 2 ? Theme.statusOkBg : (status === 1 ? Theme.statusWarnBg : Theme.statusNoneBg)
    }

    function matchStatusBorder(status) {
        return status === 2 ? Theme.statusOkBorder : (status === 1 ? Theme.statusWarnBorder : Theme.statusNoneBorder)
    }

    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            id: imageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: visible ? Math.min(180, Math.max(120, imageArea.width * 0.2)) : 0
                visible: root.showAdjacentPhotoPreviews
                color: Theme.background
                border.color: Theme.borderStrong

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    Repeater {
                        model: [
                            { label: qsTr("上一张"), path: root.previousPhotoPath, offset: -1,
                              upperStatus: root.previousPhotoUpperMatchStatus, lowerStatus: root.previousPhotoLowerMatchStatus },
                            { label: qsTr("下一张"), path: root.nextPhotoPath, offset: 1,
                              upperStatus: root.nextPhotoUpperMatchStatus, lowerStatus: root.nextPhotoLowerMatchStatus }
                        ]

                        delegate: Rectangle {
                            id: adjacentCell
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredHeight: 1
                            color: modelData.path !== "" ? Theme.surface : Theme.surfaceAlt
                            border.color: Theme.border

                            Image {
                                anchors.fill: parent
                                anchors.bottomMargin: adjacentLabel.height + 4
                                anchors.margins: 4
                                source: adjacentCell.modelData.path !== "" ? "file:///" + adjacentCell.modelData.path : ""
                                sourceSize.width: 384
                                sourceSize.height: 384
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                            }

                            Row {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 5
                                spacing: 4
                                visible: adjacentCell.modelData.path !== ""
                                z: 1

                                Repeater {
                                    model: [
                                        { garment: qsTr("上衣"), status: adjacentCell.modelData.upperStatus },
                                        { garment: qsTr("裤裙"), status: adjacentCell.modelData.lowerStatus }
                                    ]

                                    delegate: Button {
                                        required property var modelData
                                        width: 28
                                        height: 28
                                        text: root.matchStatusMarker(modelData.status)
                                        font.pixelSize: 18
                                        background: Rectangle {
                                            color: root.matchStatusBackground(modelData.status)
                                            border.color: root.matchStatusBorder(modelData.status)
                                            radius: 2
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.matchStatusToolTip(modelData.garment, modelData.status)
                                        onClicked: adjacentCell.modelData.offset < 0 ? root.prev() : root.next()
                                    }
                                }
                            }

                            Label {
                                id: adjacentLabel
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                text: adjacentCell.modelData.label
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 11
                                font.bold: true
                                color: adjacentCell.modelData.path !== "" ? Theme.textSecondary : Theme.textDisabled
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: adjacentCell.modelData.path !== ""
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: adjacentCell.modelData.offset < 0 ? root.prev() : root.next()
                            }
                        }
                    }
                }
            }

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
                    color: Theme.background
                    visible: preview.status !== Image.Ready
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("暂无实拍图 / No image")
                        color: Theme.textPlaceholder
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.imagePath !== ""
                    onDoubleClicked: root.openOriginal()
                }

                Column {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    spacing: 4
                    visible: root.inputTabActive
                    z: 1

                    IconButton {
                        width: 32
                        height: 32
                        iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/navigate-unmatched-up.svg"
                        toolTipText: qsTr("上一张未匹配")
                        uiStyle: root.uiStyle
                        onClicked: root.previousUnmatchedPhoto()
                    }

                    IconButton {
                        width: 32
                        height: 32
                        iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/navigate-unmatched-down.svg"
                        toolTipText: qsTr("下一张未匹配")
                        uiStyle: root.uiStyle
                        onClicked: root.nextUnmatchedPhoto()
                    }

                    IconButton {
                        width: 32
                        height: 32
                        iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/navigate-unconfirmed-up.svg"
                        toolTipText: qsTr("上一张未确认")
                        uiStyle: root.uiStyle
                        onClicked: root.previousUnconfirmedPhoto()
                    }

                    IconButton {
                        width: 32
                        height: 32
                        iconSource: "qrc:/qt/qml/GarmentStyleMatch/images/navigate-unconfirmed-down.svg"
                        toolTipText: qsTr("下一张未确认")
                        uiStyle: root.uiStyle
                        onClicked: root.nextUnconfirmedPhoto()
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: visible ? Math.min(220, Math.max(140, imageArea.width * 0.24)) : 0
                visible: root.matchedItems.length > 0
                color: Theme.background
                border.color: Theme.borderStrong

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
                            color: Theme.surface
                            border.color: Theme.border

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
                                color: Theme.textSecondary
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            color: Theme.background
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Label {
                    text: qsTr("第 %1 / %2 张").arg(root.pageCount === 0 ? 0 : root.pageIndex + 1).arg(root.pageCount)
                    Layout.leftMargin: 12
                    color: Theme.textSecondary
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.styleId
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }
            }
        }
    }
}
