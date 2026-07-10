import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string titleText: ""
    property string pendingStyle: ""

    implicitHeight: 56
    color: "#1f2c3a"

    ButtonGroup {
        id: styleButtonGroup
        exclusive: true
    }

    Menu {
        id: styleMenu

        Repeater {
            model: controller.availableUiStyles

            delegate: MenuItem {
                required property string modelData

                text: modelData
                checkable: true
                checked: modelData.toLowerCase() === controller.currentUiStyle.toLowerCase()
                ButtonGroup.group: styleButtonGroup
                onTriggered: {
                    if (controller.setCurrentUiStyle(modelData)) {
                        root.pendingStyle = modelData
                        styleRestartDialog.open()
                    }
                }
            }
        }
    }

    Dialog {
        id: styleRestartDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: qsTr("界面风格已保存")
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape

        contentItem: Label {
            text: qsTr("界面风格已设置为 %1，将在下次启动应用时生效。\n请在方便时自行重启应用。")
                  .arg(root.pendingStyle)
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: aboutDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 32)
        modal: true
        title: qsTr("关于 %1").arg(root.titleText)
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 12

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 112
                Layout.preferredHeight: 112
                source: "qrc:/qt/qml/GarmentStyleMatch/images/GarmentStyleMatch.png"
                sourceSize.width: 112
                sourceSize.height: 112
                fillMode: Image.PreserveAspectFit
                mipmap: true
            }

            Label {
                Layout.fillWidth: true
                text: root.titleText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("服装模特图与手绘图匹配分类工具")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: "#5f6b76"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 8

                Label { text: qsTr("版本号"); font.bold: true }
                Label { text: Qt.application.version }

                Label {
                    text: qsTr("版权信息")
                    font.bold: true
                    Layout.alignment: Qt.AlignTop
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("© 2026 Eidos。保留所有权利。")
                    wrapMode: Text.Wrap
                }

                Label {
                    text: qsTr("开发商")
                    font.bold: true
                    Layout.alignment: Qt.AlignTop
                }
                Label {
                    Layout.fillWidth: true
                    text: "Eidos / Fan Yang"
                    wrapMode: Text.Wrap
                }

                Label {
                    text: qsTr("联系方式")
                    font.bold: true
                    Layout.alignment: Qt.AlignTop
                }
                Label {
                    Layout.fillWidth: true
                    text: "<a href=\"mailto:missdeer@gmail.com\">missdeer@gmail.com</a><br>"
                          + "<a href=\"https://github.com/missdeer/GarmentStyleMatch\">"
                          + "github.com/missdeer/GarmentStyleMatch</a>"
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Label {
            text: root.titleText
            color: "white"
            font.pixelSize: 20
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        Label {
            id: styleLink
            text: qsTr("风格")
            color: styleMouse.containsMouse ? "#ffffff" : "#b9d8f2"
            font.pixelSize: 13
            font.underline: true

            MouseArea {
                id: styleMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: styleMenu.popup()
            }
        }

        Label {
            id: aboutLink
            text: qsTr("关于")
            color: aboutMouse.containsMouse ? "#ffffff" : "#b9d8f2"
            font.pixelSize: 13
            font.underline: true

            MouseArea {
                id: aboutMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: aboutDialog.open()
            }
        }
    }
}
