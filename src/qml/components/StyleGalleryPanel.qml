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
    property var categoryRuleOptions: []
    property string currentCategoryRule: ""
    property string categoryRuleStatus: ""
    property string categorySummary: ""
    readonly property var categoryOptions: [
        { value: "全部", text: qsTr("全部") },
        { value: "upper", text: qsTr("上衣") },
        { value: "lower", text: qsTr("裤裙") },
        { value: "accessory", text: qsTr("配件") },
        { value: "dress", text: qsTr("连衣裙") },
        { value: "unknown", text: qsTr("未知") }
    ]

    signal searchTextEdited(string text)
    signal categoryEdited(string text)
    signal categoryRuleSelected(string ruleId)
    signal reloadCategoryRuleRequested()
    signal matchRequested(int galleryRow, string part)
    signal confirmRequested(int galleryRow, string part)
    signal categoryOverrideRequested(int galleryRow, string part)

    color: Theme.background

    function urlToLocalPath(u) {
        var s = u.toString().replace(/^file:\/\//, "")
        if (/^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        return s
    }

    function showUpperActions(part) {
        return part !== "lower" && part !== "accessory" && part !== "dress"
    }

    function showLowerActions(part) {
        return part !== "upper" && part !== "accessory" && part !== "dress"
    }

    function showDressActions(part) {
        return part === "dress"
    }

    function categoryDisplayName(part) {
        for (let index = 0; index < root.categoryOptions.length; ++index) {
            const option = root.categoryOptions[index]
            if (option.value === part)
                return option.text
        }
        return part
    }

    function categoryRuleIndex() {
        const options = root.categoryRuleOptions || []
        let fallbackIndex = options.length > 0 ? 0 : -1
        for (let i = 0; i < options.length; ++i) {
            if (options[i].id === root.currentCategoryRule)
                return i
            if (options[i].id === "")
                fallbackIndex = i
        }
        return fallbackIndex
    }

    function categoryIndex() {
        for (let index = 0; index < root.categoryOptions.length; ++index) {
            if (root.categoryOptions[index].value === root.categoryText)
                return index
        }
        return 0
    }

    function invalidateMatchMenu() {
        matchMenu.close()
        matchMenu.galleryRow = -1
        matchMenu.part = "unknown"
    }

    function openMatchMenu(galleryRow, part) {
        matchMenu.close()
        matchMenu.galleryRow = galleryRow
        matchMenu.part = part
        matchMenu.popup()
    }

    Connections {
        target: root.model
        function onModelAboutToBeReset() {
            root.invalidateMatchMenu()
        }
    }

    Menu {
        id: matchMenu
        objectName: "galleryMatchMenu"
        property int galleryRow: -1
        property string part: "unknown"
        readonly property var actionEntries: {
            const entries = []
            if (root.currentPhotoSelected && root.showUpperActions(part))
                entries.push({ objectName: "galleryMatchUpperMenuItem", text: qsTr("匹配为上衣"), action: "match", part: "upper" })
            if (root.currentPhotoSelected && root.showLowerActions(part))
                entries.push({ objectName: "galleryMatchLowerMenuItem", text: qsTr("匹配为裤裙"), action: "match", part: "lower" })
            if (root.currentPhotoSelected && root.showDressActions(part))
                entries.push({ objectName: "galleryMatchDressMenuItem", text: qsTr("匹配为连衣裙"), action: "match", part: "dress" })
            if (root.currentPhotoSelected && root.showUpperActions(part))
                entries.push({ objectName: "galleryConfirmUpperMenuItem", text: qsTr("确认为上衣"), action: "confirm", part: "upper" })
            if (root.currentPhotoSelected && root.showLowerActions(part))
                entries.push({ objectName: "galleryConfirmLowerMenuItem", text: qsTr("确认为裤裙"), action: "confirm", part: "lower" })
            if (root.currentPhotoSelected && root.showDressActions(part))
                entries.push({ objectName: "galleryConfirmDressMenuItem", text: qsTr("确认为连衣裙"), action: "confirm", part: "dress" })
            return entries
        }

        Instantiator {
            model: matchMenu.actionEntries

            delegate: MenuItem {
                required property var modelData
                objectName: modelData.objectName
                text: modelData.text
                onTriggered: {
                    if (modelData.action === "match")
                        root.matchRequested(matchMenu.galleryRow, modelData.part)
                    else
                        root.confirmRequested(matchMenu.galleryRow, modelData.part)
                }
            }

            onObjectAdded: function(index, object) {
                matchMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                matchMenu.removeItem(object)
            }
        }

        Menu {
            id: categoryOverrideMenu
            objectName: "galleryCategoryOverrideMenu"
            title: qsTr("设置品类")

            MenuItem {
                objectName: "gallerySetCategory-upper"
                text: qsTr("上衣")
                onTriggered: root.categoryOverrideRequested(matchMenu.galleryRow, "upper")
            }
            MenuItem {
                objectName: "gallerySetCategory-lower"
                text: qsTr("裤裙")
                onTriggered: root.categoryOverrideRequested(matchMenu.galleryRow, "lower")
            }
            MenuItem {
                objectName: "gallerySetCategory-dress"
                text: qsTr("连衣裙")
                onTriggered: root.categoryOverrideRequested(matchMenu.galleryRow, "dress")
            }
            MenuItem {
                objectName: "gallerySetCategory-accessory"
                text: qsTr("配件")
                onTriggered: root.categoryOverrideRequested(matchMenu.galleryRow, "accessory")
            }
            MenuItem {
                objectName: "gallerySetCategory-unknown"
                text: qsTr("未知")
                onTriggered: root.categoryOverrideRequested(matchMenu.galleryRow, "unknown")
            }
        }
    }

    Dialog {
        id: categorySummaryDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 32)
        height: Math.min(520, parent.height - 32)
        modal: true
        title: qsTr("款号品类分类摘要")
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape

        contentItem: ScrollView {
            TextArea {
                text: root.categorySummary
                readOnly: true
                wrapMode: TextEdit.Wrap
                selectByMouse: true
            }
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
            implicitHeight: 72
            color: Theme.background

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label { text: qsTr("品类规则") }
                    ComboBox {
                        id: categoryRuleBox
                        objectName: "categoryRuleBox"
                        Layout.fillWidth: true
                        model: root.categoryRuleOptions
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: root.categoryRuleIndex()
                        onActivated: root.categoryRuleSelected(currentValue)
                    }
                    Button {
                        text: qsTr("重新加载")
                        enabled: root.currentCategoryRule !== ""
                        onClicked: root.reloadCategoryRuleRequested()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        Layout.fillWidth: true
                        text: root.categoryRuleStatus
                        elide: Label.ElideRight
                        color: Theme.textSecondary
                    }
                    Button {
                        text: qsTr("查看摘要")
                        onClicked: categorySummaryDialog.open()
                    }
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
                    objectName: "galleryPartFilterBox"
                    model: root.categoryOptions
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: root.categoryIndex()
                    Layout.preferredWidth: 100
                    onActivated: root.categoryEdited(currentValue)
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
            readonly property real wheelScrollMultiplier: 3.0

            function scrollBy(distance) {
                const minimumContentY = originY
                const maximumContentY = Math.max(minimumContentY,
                                                  originY + contentHeight - height)
                contentY = Math.max(minimumContentY,
                                    Math.min(maximumContentY, contentY + distance))
            }

            delegate: Item {
                id: cell
                objectName: "galleryCell-" + index
                required property int    index
                required property string styleId
                required property string imagePath
                required property int    indexLabel
                required property string part

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
                                objectName: "galleryMouseArea-" + cell.index
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor
                                z: 1
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton)
                                        root.openMatchMenu(cell.index, cell.part)
                                }
                                onDoubleClicked: function(mouse) {
                                    if (mouse.button === Qt.LeftButton)
                                        root.searchTextEdited(cell.styleId)
                                }
                            }

                            Grid {
                                objectName: "galleryActionGrid-" + cell.index
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 4
                                columns: 2
                                spacing: 4
                                visible: thumbHover.hovered
                                z: 2

                                Button {
                                    id: matchUpperButton
                                    objectName: "galleryMatchUpperButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showUpperActions(cell.part)
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
                                    objectName: "galleryConfirmUpperButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showUpperActions(cell.part)
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
                                    objectName: "galleryMatchLowerButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showLowerActions(cell.part)
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
                                    objectName: "galleryConfirmLowerButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showLowerActions(cell.part)
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

                                Button {
                                    id: matchDressButton
                                    objectName: "galleryMatchDressButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showDressActions(cell.part)
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-match-dress.svg"
                                            color: matchDressButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式匹配为当前实拍图的连衣裙（待确认）")
                                    onClicked: root.matchRequested(cell.index, "dress")
                                }

                                Button {
                                    id: confirmDressButton
                                    objectName: "galleryConfirmDressButton-" + cell.index
                                    width: 28
                                    height: 28
                                    leftPadding: 2
                                    rightPadding: 2
                                    topPadding: 2
                                    bottomPadding: 2
                                    enabled: root.currentPhotoSelected
                                    visible: root.showDressActions(cell.part)
                                    contentItem: Item {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        ColorImage {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            source: "qrc:/qt/qml/GarmentStyleMatch/images/gallery-confirm-dress.svg"
                                            color: confirmDressButton.palette.buttonText
                                            sourceSize.width: 24
                                            sourceSize.height: 24
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("将此款式确认为当前实拍图的连衣裙")
                                    onClicked: root.confirmRequested(cell.index, "dress")
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
                                    objectName: "galleryPartLabel-" + cell.index
                                    anchors.centerIn: parent
                                    text: root.categoryDisplayName(cell.part)
                                    font.pixelSize: 10
                                    color: Theme.accent
                                }
                            }
                        }
                    }

                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                z: 2

                onWheel: (wheel) => {
                    let delta = wheel.pixelDelta.y
                    if (delta === 0)
                        delta = wheel.angleDelta.y
                    grid.scrollBy(-delta * grid.wheelScrollMultiplier)
                    wheel.accepted = true
                }
            }
        }
    }
}
