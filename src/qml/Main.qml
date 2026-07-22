import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Universal
import QtQuick.Layouts
import QtQuick.Window

import GarmentStyleMatch

import "components"

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visibility: Window.Maximized
    title: controller.title

    // When the user picks the Material or Universal QQC2 style, follow the
    // system's color scheme. FluentWinUI3 and Fusion already do this natively;
    // the two attached-property lines below are no-ops under those styles.
    Material.theme: Material.System
    Universal.theme: Universal.System

    font.family: {
        switch (Qt.platform.os) {
        case "windows": return "Microsoft YaHei UI"
        case "osx":     return "PingFang SC"
        case "linux":   return "Noto Sans CJK SC"
        default:        return Qt.application.font.family
        }
    }
    font.pixelSize: 13

    property bool firstFramePresented: false

    onFrameSwapped: {
        if (firstFramePresented)
            return
        firstFramePresented = true
        controller.notifyMainWindowShown()
        workspaceLoader.active = true
        Qt.callLater(function() { controller.completeDeferredStartup() })
    }

    property string statusText: qsTr("就绪")
    property int pendingCopyOffset: 0
    property string pendingCopyPart: ""
    property bool pendingCopyToAdjacent: false
    property int pendingGalleryMatchRow: -1
    property string pendingGalleryMatchPart: ""
    property bool pendingGalleryMatchConfirmed: false
    property string classificationReportTitle: ""
    property string classificationReportText: ""

    component SplitterHandle: Rectangle {
        implicitWidth: 5
        implicitHeight: 5
        color: SplitHandle.pressed ? Theme.splitterPressed
              : (SplitHandle.hovered ? Theme.splitterHover : Theme.splitterIdle)
    }

    function requestStyleIdCopy(offset, part, toAdjacent) {
        if (!controller.copyWouldOverwriteConfirmedStyleIds(offset, part, toAdjacent)) {
            if (toAdjacent)
                controller.copyStyleIdsToAdjacent(offset, part, "cancel")
            else
                controller.copyAdjacentStyleIds(offset, part, "cancel")
            return
        }
        pendingCopyOffset = offset
        pendingCopyPart = part
        pendingCopyToAdjacent = toAdjacent
        overwriteConfirmedStyleIdsDialog.open()
    }

    function copyPendingStyleIds(confirmedPolicy) {
        if (pendingCopyToAdjacent)
            controller.copyStyleIdsToAdjacent(pendingCopyOffset, pendingCopyPart, confirmedPolicy)
        else
            controller.copyAdjacentStyleIds(pendingCopyOffset, pendingCopyPart, confirmedPolicy)
    }

    function requestGalleryMatch(galleryRow, part, confirmed) {
        if (!controller.galleryMatchWouldOverwriteConfirmedStyleId(part)) {
            controller.matchGalleryItemToCurrentPhoto(galleryRow, part, false, confirmed)
            return
        }
        pendingGalleryMatchRow = galleryRow
        pendingGalleryMatchPart = part
        pendingGalleryMatchConfirmed = confirmed
        overwriteGalleryMatchDialog.open()
    }

    Connections {
        target: controller
        function onLogMessage(msg) { root.statusText = msg }
        function onClassificationFinished(title, report) {
            root.classificationReportTitle = title
            root.classificationReportText = report
            classificationReportDialog.open()
        }
    }

    Dialog {
        id: classificationReportDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 32)
        modal: true
        title: root.classificationReportTitle
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape

        contentItem: Label {
            text: root.classificationReportText
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: overwriteConfirmedStyleIdsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        modal: true
        title: qsTr("确认覆盖款号")
        standardButtons: Dialog.Yes | Dialog.Apply | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape
        onOpened: {
            standardButton(Dialog.Yes).text = qsTr("覆盖全部")
            standardButton(Dialog.Apply).text = qsTr("仅覆盖未确认项")
            standardButton(Dialog.Cancel).text = qsTr("取消")
        }
        onAccepted: root.copyPendingStyleIds("overwriteAll")
        onApplied: {
            root.copyPendingStyleIds("unconfirmedOnly")
            close()
        }

        contentItem: Label {
            text: qsTr("目标实拍图已有被确认的款号，请选择复制方式。")
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: overwriteGalleryMatchDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: qsTr("确认覆盖款号")
        standardButtons: Dialog.Yes | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape
        onOpened: {
            standardButton(Dialog.Yes).text = qsTr("覆盖")
            standardButton(Dialog.Cancel).text = qsTr("取消")
        }
        onAccepted: controller.matchGalleryItemToCurrentPhoto(
                        root.pendingGalleryMatchRow,
                        root.pendingGalleryMatchPart,
                        true,
                        root.pendingGalleryMatchConfirmed)

        contentItem: Label {
            text: qsTr("当前实拍图已有已确认的%1款号，是否覆盖？")
                    .arg(root.pendingGalleryMatchPart === "upper" ? qsTr("上衣") : qsTr("裤裙"))
            wrapMode: Text.Wrap
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            Layout.fillWidth: true
            titleText: controller.title
        }

        Loader {
            id: workspaceLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: false
            asynchronous: true

            onLoaded: Qt.callLater(function() {
                controller.restorePersistentState()
            })

            sourceComponent: SplitView {
                anchors.fill: parent
                orientation: Qt.Horizontal
                handle: SplitterHandle {}

                CandidatePanel {
                    id: candidatePanel
                    SplitView.preferredWidth: 320
                    SplitView.minimumWidth: 220
                    outputModel: candidateModel
                    inputModel:  photoModel
                    currentRow:      controller.currentIndex
                    currentPhotoRow: controller.currentPhotoIndex
                    inputTabActive: controller.inputTabActive
                    photoDir:   controller.photoDir
                    outputDir:  controller.outputDir
                    inputFilterText: controller.inputFilterText
                    outputFilterText: controller.outputFilterText
                    onRowActivated:             (row) => controller.currentIndex = row
                    onPhotoRowActivated:        (row) => controller.currentPhotoIndex = row
                    onPhotoDirEdited:           (p)   => controller.photoDir = p
                    onOutputDirEdited:           (p)   => controller.outputDir = p
                    onInputFilterTextEdited:     (t)   => controller.inputFilterText = t
                    onOutputFilterTextEdited:    (t)   => controller.outputFilterText = t
                    onInputTabActiveEdited:      (active) => controller.activatePreview(active)
                }

                SplitView {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320
                    orientation: Qt.Vertical
                    handle: SplitterHandle {}

                    ColumnLayout {
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 220
                        spacing: 0

                        MainImageView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            imagePath:       controller.currentImagePath
                            styleId:         controller.currentStyleId
                            matchedItems:     controller.autoMatchedItems
                            inputTabActive: controller.inputTabActive
                            uiStyle: controller.currentUiStyle
                            showAdjacentPhotoPreviews: candidatePanel.inputTabActive
                            previousPhotoPath: controller.previousPhotoPath
                            nextPhotoPath:     controller.nextPhotoPath
                            previousPhotoUpperMatchStatus: controller.previousPhotoUpperMatchStatus
                            previousPhotoLowerMatchStatus: controller.previousPhotoLowerMatchStatus
                            nextPhotoUpperMatchStatus: controller.nextPhotoUpperMatchStatus
                            nextPhotoLowerMatchStatus: controller.nextPhotoLowerMatchStatus
                            pageIndex:       controller.currentImagePage
                            pageCount:       controller.currentImageCount
                            previousEnabled: candidatePanel.inputTabActive
                                             ? controller.currentPhotoIndex > 0
                                             : controller.currentImagePage > 0
                            nextEnabled:     candidatePanel.inputTabActive
                                             ? controller.currentPhotoIndex >= 0
                                               && controller.currentPhotoIndex + 1 < candidatePanel.inputItemCount
                                             : controller.currentImagePage + 1 < controller.currentImageCount
                            onPrev:          controller.previousImage(candidatePanel.inputTabActive)
                            onNext:          controller.nextImage(candidatePanel.inputTabActive)
                            onPreviousUnmatchedPhoto: controller.previousUnmatchedPhoto()
                            onNextUnmatchedPhoto: controller.nextUnmatchedPhoto()
                            onPreviousUnconfirmedPhoto: controller.previousUnconfirmedPhoto()
                            onNextUnconfirmedPhoto: controller.nextUnconfirmedPhoto()
                            onOpenOriginal:  controller.openCurrentImageExternally()
                            onMatchConfirmed: (part) => controller.confirmAutoMatch(part)
                            onMatchRejected:  (part) => controller.rejectAutoMatch(part)
                        }

                        OutputImagePreview {
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? 88 : 0
                            visible: !candidatePanel.inputTabActive
                                     && controller.currentIndex >= 0
                                     && controller.currentImageCount > 0
                            imagePaths: controller.currentOutputImagePaths
                            currentIndex: controller.currentImagePage
                            onImageActivated: (index) => controller.currentImagePage = index
                        }
                    }

                    ImagePropertiesPanel {
                        SplitView.preferredHeight: 265
                        SplitView.minimumHeight: 170
                        busy: controller.busy
                        batchAutoMatchInProgress: controller.batchAutoMatchInProgress
                        previousPhotoAvailable: candidatePanel.inputTabActive
                                                && controller.currentPhotoIndex > 0
                        nextPhotoAvailable: candidatePanel.inputTabActive
                                            && controller.currentPhotoIndex >= 0
                                            && controller.currentPhotoIndex + 1 < candidatePanel.inputItemCount
                        onAutoMatchRequested: controller.autoMatchStyleIds()
                        onAutoMatchAllRequested: controller.autoMatchAllStyleIds()
                        onAutoMatchAllUnconfirmedRequested: controller.autoMatchAllUnconfirmedStyleIds()
                        onCancelAutoMatchAllRequested: controller.cancelAutoMatchAllStyleIds()
                        onCopyStyleIdsRequested: (offset, part) => root.requestStyleIdCopy(offset, part, false)
                        onCopyStyleIdsToAdjacentRequested: (offset, part) => root.requestStyleIdCopy(offset, part, true)
                        onClassifyMatchedPhotosRequested: controller.classifyMatchedPhotos()
                        onClassifyConfirmedPhotosRequested: controller.classifyConfirmedPhotos()
                    }
                }

                GalleryPanel {
                    SplitView.preferredWidth: 440
                    SplitView.minimumWidth: 280
                    styleGalleryModel: galleryModel
                    pagesModel:       pptPageModel
                    pptSelectedCount: pptPageModel.selectedCount
                    busy:             controller.busy
                    categoryText:     controller.categoryFilter
                    searchText:       controller.searchText
                    pptPath:          controller.pptPath
                    currentPhotoSelected: controller.inputTabActive && controller.currentPhotoIndex >= 0
                    categoryRuleOptions: controller.availableCategoryRules
                    currentCategoryRule: controller.currentCategoryRule
                    categoryRuleStatus: controller.categoryRuleStatus
                    categorySummary: controller.categorySummary
                    onSearchTextEdited:   (t) => controller.searchText = t
                    onCategoryEdited:     (t) => controller.categoryFilter = t
                    onCategoryRuleSelected: (ruleId) => controller.currentCategoryRule = ruleId
                    onReloadCategoryRuleRequested: controller.reloadCategoryRule()
                    onPptPathEdited:      (p) => controller.pptPath = p
                    onPptSearchRequested: ()  => controller.reloadPpt()
                    onPptPageToggled:     (r) => controller.togglePptPageSelected(r)
                    onExtractRequested:   ()  => controller.extractFromSelectedPages()
                    onGalleryMatchRequested: (row, part) => root.requestGalleryMatch(row, part, false)
                    onGalleryConfirmRequested: (row, part) => root.requestGalleryMatch(row, part, true)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 26
            color: Theme.statusBg
            border.color: Theme.statusBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                BusyIndicator {
                    running: controller.busy
                    visible: controller.busy
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }
                Label {
                    text: root.statusText
                    color: Theme.statusText
                    font.pixelSize: 12
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: controller.busy ? qsTr("处理中…") : qsTr("空闲")
                    color: controller.busy ? Theme.statusBusy : Theme.statusIdle
                    font.pixelSize: 12
                }
            }
        }
    }
}
