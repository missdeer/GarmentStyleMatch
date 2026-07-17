import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root
    property string titleText: ""
    property string pendingStyle: ""
    property string pendingInferenceEngine: ""

    implicitHeight: 56
    color: Theme.headerBg

    ButtonGroup {
        id: styleButtonGroup
        exclusive: true
    }

    ButtonGroup {
        id: inferenceEngineButtonGroup
        exclusive: true
    }

    ButtonGroup {
        id: parallelMatchThreadButtonGroup
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
                else if (controller.modelFilesExist())
                    overwriteModelsDialog.open()
                else
                    controller.downloadModels()
            }
        }

        MenuItem {
            text: qsTr("打开模型目录")
            onTriggered: controller.openModelDirectory()
        }
    }

    Menu {
        id: parallelMatchThreadMenu

        Repeater {
            model: 8

            delegate: MenuItem {
                required property int index

                text: qsTr("%1 线程").arg(index + 1)
                checkable: true
                checked: controller.parallelMatchThreadCount === index + 1
                enabled: !controller.busy
                ButtonGroup.group: parallelMatchThreadButtonGroup
                onTriggered: controller.parallelMatchThreadCount = index + 1
            }
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
        id: overwriteModelsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        modal: true
        title: qsTr("确认重新下载模型")
        standardButtons: Dialog.Yes | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape
        onOpened: {
            standardButton(Dialog.Yes).text = qsTr("重新下载并覆盖")
            standardButton(Dialog.Cancel).text = qsTr("取消")
        }
        onAccepted: controller.downloadModels()

        contentItem: Label {
            text: qsTr("检测到已有模型文件。是否确认重新下载并覆盖原有的模型文件？")
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

    Dialog {
        id: windowsMlEpDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 32)
        height: Math.min(480, parent.height - 32)
        modal: true
        title: qsTr("Windows ML 执行提供程序")
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape

        onOpened: controller.refreshWindowsMlExecutionProviders()

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("显示 Windows ML 在本机检测到的兼容 EP。下载由 Windows Update 完成；动态 EP 需要 Windows 11 24H2 或更高版本及兼容驱动。")
                wrapMode: Text.Wrap
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: controller.windowsMlEpOperationInProgress
                visible: running
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Column {
                    width: windowsMlEpDialog.availableWidth
                    spacing: 8

                    Label {
                        width: parent.width
                        visible: controller.windowsMlExecutionProviders.length === 0
                        text: qsTr("本机没有 Windows ML catalog 返回的兼容动态 EP。")
                        wrapMode: Text.Wrap
                        color: Theme.textMuted
                    }

                    Repeater {
                        model: controller.windowsMlExecutionProviders

                        delegate: Frame {
                            required property var modelData
                            width: parent.width

                            RowLayout {
                                width: parent.width
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label { text: modelData.name; font.bold: true }
                                    Label {
                                        text: qsTr("版本：%1　状态：%2").arg(modelData.version || qsTr("未知")).arg(modelData.state)
                                        color: Theme.textMuted
                                    }
                                }

                                Button {
                                    Layout.preferredWidth: 128
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    text: modelData.installed ? qsTr("设为推理引擎") : qsTr("下载")
                                    enabled: !controller.windowsMlEpOperationInProgress
                                    onClicked: {
                                        if (modelData.installed) {
                                            if (controller.useWindowsMlExecutionProvider(modelData.name)) {
                                                root.pendingInferenceEngine = "Windows ML · " + modelData.name
                                                inferenceEngineRestartDialog.open()
                                            }
                                        } else {
                                            controller.installWindowsMlExecutionProvider(modelData.name)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
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
                color: Theme.textMuted
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
            color: Theme.headerText
            font.pixelSize: 20
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        Label {
            id: inferenceEngineLink
            text: qsTr("推理引擎：%1").arg(controller.currentInferenceEngine)
            color: inferenceEngineMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
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
            id: parallelMatchThreadLink
            text: qsTr("匹配线程：%1").arg(controller.parallelMatchThreadCount)
            color: parallelMatchThreadMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
            font.pixelSize: 13
            font.underline: true
            ToolTip.visible: parallelMatchThreadMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: qsTr("推荐线程数：CPU 1，CoreML / DirectML 2，CUDA / TensorRT 4；Windows ML EP 参考对应引擎（CPU / DirectML / CUDA）")

            MouseArea {
                id: parallelMatchThreadMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: controller.busy ? Qt.ArrowCursor : Qt.PointingHandCursor
                onClicked: {
                    if (!controller.busy)
                        parallelMatchThreadMenu.popup()
                }
            }
        }

        Label {
            id: windowsMlEpLink
            visible: Qt.platform.os === "windows"
            text: qsTr("Windows ML EP")
            color: windowsMlEpMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
            font.pixelSize: 13
            font.underline: true

            MouseArea {
                id: windowsMlEpMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: windowsMlEpDialog.open()
            }
        }

        Label {
            id: downloadModelsLink
            text: controller.modelDownloadInProgress ? qsTr("模型下载中...") : qsTr("下载模型")
            color: downloadModelsMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
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
            color: styleMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
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
            color: aboutMouse.containsMouse ? Theme.headerLinkHover : Theme.headerLink
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
