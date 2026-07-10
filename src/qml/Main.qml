import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import "components"

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: controller.title

    font.family: {
        switch (Qt.platform.os) {
        case "windows": return "Microsoft YaHei UI"
        case "osx":     return "PingFang SC"
        case "linux":   return "Noto Sans CJK SC"
        default:        return Qt.application.font.family
        }
    }
    font.pixelSize: 13

    Component.onCompleted: {
        controller.loadDemoData()
        controller.restorePersistentState()
    }

    property string statusText: qsTr("就绪")

    Connections {
        target: controller
        function onLogMessage(msg) { root.statusText = msg }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            Layout.fillWidth: true
            titleText: controller.title
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            CandidatePanel {
                id: candidatePanel
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                outputModel: candidateModel
                inputModel:  photoModel
                currentRow:      controller.currentIndex
                currentPhotoRow: controller.currentPhotoIndex
                photoDir:   controller.photoDir
                outputDir:  controller.outputDir
                onRowActivated:             (row) => controller.currentIndex = row
                onPhotoRowActivated:        (row) => controller.currentPhotoIndex = row
                onPhotoDirEdited:           (p)   => controller.photoDir = p
                onPhotoDirSearchRequested:  ()    => controller.scanPhotoDir()
                onOutputDirEdited:          (p)   => controller.outputDir = p
                onOutputDirSearchRequested: ()    => controller.scanOutputDir()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                MainImageView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    imagePath:       controller.currentImagePath
                    styleId:         controller.currentStyleId
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
                    onOpenOriginal:  controller.openCurrentImageExternally()
                }

                ImagePropertiesPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 245
                    Layout.minimumHeight: 170
                    styleId: controller.currentStyleId
                    onConfirmSelected: (idx) => controller.confirmSelectedThumb(idx)
                    onConfirmStyleId:  (id)  => controller.confirmStyleId(id)
                    onPreviousItem:    controller.previousCandidate()
                    onGenerateModel:   controller.generateFineTuneModel()
                }
            }

            GalleryPanel {
                Layout.preferredWidth: 440
                Layout.fillHeight: true
                styleGalleryModel: galleryModel
                pagesModel:       pptPageModel
                pptSelectedCount: pptPageModel.selectedCount
                busy:             controller.busy
                categoryText:     controller.categoryFilter
                searchText:       controller.searchText
                pptPath:          controller.pptPath
                onSearchTextEdited:   (t) => controller.searchText = t
                onCategoryEdited:     (t) => controller.categoryFilter = t
                onPptPathEdited:      (p) => controller.pptPath = p
                onPptSearchRequested: ()  => controller.reloadPpt()
                onPptPageToggled:     (r) => controller.togglePptPageSelected(r)
                onExtractRequested:   ()  => controller.extractFromSelectedPages()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 26
            color: "#e9edf1"
            border.color: "#dee3e8"

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
                    color: "#3a4a5a"
                    font.pixelSize: 12
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: controller.busy ? qsTr("处理中…") : qsTr("空闲")
                    color: controller.busy ? "#2f5aa8" : "#6b7a89"
                    font.pixelSize: 12
                }
            }
        }
    }
}
