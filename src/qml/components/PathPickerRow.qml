import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    property string placeholderText: ""
    property string path: ""
    property bool   isFile: false               // true = 文件选择,false = 目录选择
    property string dialogTitle: qsTr("选择")
    property var    nameFilters: ["All files (*)"]
    property string pickLabel:   isFile ? qsTr("选择文件") : qsTr("选择目录")
    property string searchLabel: qsTr("搜索")
    property string pickIconName: "folder-open"
    property string searchIconName: "edit-find"
    property string uiStyle: ""
    readonly property bool useCompactIconPadding: {
        const style = uiStyle.toLowerCase()
        return style === "fluentwinui3" || style === "imagine"
    }

    signal pathPicked(string path)
    signal searchRequested()

    implicitHeight: row.implicitHeight

    RowLayout {
        id: row
        anchors.left:  parent.left
        anchors.right: parent.right
        spacing: 4

        TextField {
            id: field
            Layout.fillWidth: true
            text: root.path
            placeholderText: root.placeholderText
            onEditingFinished: root.pathPicked(text)
        }
        Button {
            id: pickButton
            readonly property int iconExtent: Math.max(
                1, Math.floor(Math.min(availableWidth, availableHeight)))

            Binding on leftPadding {
                when: root.useCompactIconPadding
                value: 8
            }
            Binding on rightPadding {
                when: root.useCompactIconPadding
                value: 8
            }

            Layout.preferredWidth: field.implicitHeight
            Layout.preferredHeight: field.implicitHeight
            display: AbstractButton.IconOnly
            icon.name: root.pickIconName
            icon.width: iconExtent
            icon.height: iconExtent
            ToolTip.visible: hovered
            ToolTip.text: root.pickLabel
            onClicked: root.isFile ? fileDlg.open() : folderDlg.open()
        }
        Button {
            id: searchButton
            readonly property int iconExtent: Math.max(
                1, Math.floor(Math.min(availableWidth, availableHeight)))

            Binding on leftPadding {
                when: root.useCompactIconPadding
                value: 8
            }
            Binding on rightPadding {
                when: root.useCompactIconPadding
                value: 8
            }

            Layout.preferredWidth: field.implicitHeight
            Layout.preferredHeight: field.implicitHeight
            display: AbstractButton.IconOnly
            icon.name: root.searchIconName
            icon.width: iconExtent
            icon.height: iconExtent
            ToolTip.visible: hovered
            ToolTip.text: root.searchLabel
            onClicked: root.searchRequested()
        }
    }

    FolderDialog {
        id: folderDlg
        title: root.dialogTitle
        onAccepted: {
            const p = selectedFolder.toString().replace(/^file:\/{2,3}/, "")
            field.text = p
            root.pathPicked(p)
        }
    }
    FileDialog {
        id: fileDlg
        title: root.dialogTitle
        nameFilters: root.nameFilters
        onAccepted: {
            const p = selectedFile.toString().replace(/^file:\/{2,3}/, "")
            field.text = p
            root.pathPicked(p)
        }
    }
}
