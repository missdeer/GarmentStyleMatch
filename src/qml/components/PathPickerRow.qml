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
            text: root.pickLabel
            onClicked: root.isFile ? fileDlg.open() : folderDlg.open()
        }
        Button {
            text: root.searchLabel
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
