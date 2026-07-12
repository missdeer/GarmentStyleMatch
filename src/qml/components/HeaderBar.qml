import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string titleText: ""
    property string pendingStyle: ""
    property string pendingInferenceEngine: ""

    implicitHeight: 56
    color: "#1f2c3a"

    ButtonGroup {
        id: styleButtonGroup
        exclusive: true
    }

    ButtonGroup {
        id: inferenceEngineButtonGroup
        exclusive: true
    }

    Menu {
        id: inferenceEngineMenu

        Repeater {
            model: controller.availableInferenceEngines

            delegate: MenuItem {
                required property string modelData

                text: modelData
                checkable: true
                checked: modelData === controller.currentInferenceEngine
                ButtonGroup.group: inferenceEngineButtonGroup
                onTriggered: {
                    if (controller.setCurrentInferenceEngine(modelData)) {
                        root.pendingInferenceEngine = modelData
                        inferenceEngineRestartDialog.open()
                    }
                }
            }
        }
    }

    Menu {
        id: downloadModelsMenu

        MenuItem {
            text: controller.modelDownloadInProgress
                  ? qsTr("停止下载")
                  : (controller.modelsAvailable ? qsTr("重新下载模型") : qsTr("下载服装匹配模型"))
            onTriggered: {
                if (controller.modelDownloadInProgress)
                    controller.cancelModelDownload()
                else
                    controller.downloadModels()
            }
        }

        MenuItem {
            text: qsTr("打开模型目录")
            onTriggered: controller.openModelDirectory()
        }
    }

    Connections {
        target: controller
        function onModelDownloadRequired() { missingModelsDialog.open() }
    }

    Dialog {
        id: missingModelsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: qsTr("缺少匹配模型")
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape

        contentItem: Label {
            text: qsTr("未找到可用的服装匹配模型。请从顶部“下载模型”菜单触发下载。")
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: inferenceEngineRestartDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: qsTr("推理引擎已保存")
        standardButtons: Dialog.Ok
        closePolicy: Popup.NoAutoClose

        contentItem: Label {
            text: qsTr("推理引擎已设置为 %1。为避免加载不同版本的 ONNX Runtime，必须重启应用后才能生效。")
                  .arg(root.pendingInferenceEngine)
            wrapMode: Text.Wrap
        }
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
            id: inferenceEngineLink
            text: qsTr("推理引擎：%1").arg(controller.currentInferenceEngine)
            color: inferenceEngineMouse.containsMouse ? "#ffffff" : "#b9d8f2"
            font.pixelSize: 13
            font.underline: true

            MouseArea {
                id: inferenceEngineMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: inferenceEngineMenu.popup()
            }
        }

        Label {
            id: downloadModelsLink
            text: controller.modelDownloadInProgress ? qsTr("模型下载中...") : qsTr("下载模型")
            color: downloadModelsMouse.containsMouse ? "#ffffff" : "#b9d8f2"
            font.pixelSize: 13
            font.underline: true

            MouseArea {
                id: downloadModelsMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: downloadModelsMenu.popup()
            }
        }

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
