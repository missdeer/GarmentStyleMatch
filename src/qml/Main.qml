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
    title: controller.title + " - " + controller.subtitle

    font.family: {
        switch (Qt.platform.os) {
        case "windows": return "Microsoft YaHei UI"
        case "osx":     return "PingFang SC"
        case "linux":   return "Noto Sans CJK SC"
        default:        return Qt.application.font.family
        }
    }
    font.pixelSize: 13

    Component.onCompleted: controller.loadDemoData()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            Layout.fillWidth: true
            titleText:    controller.title
            subtitleText: controller.subtitle
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            CandidatePanel {
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
                    onPrev:          controller.previousImage()
                    onNext:          controller.nextImage()
                    onOpenOriginal:  controller.openCurrentImageExternally()
                }

                ConfirmBar {
                    Layout.fillWidth: true
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
                model: galleryModel
                categoryText: controller.categoryFilter
                searchText:   controller.searchText
                pptPath:      controller.pptPath
                onSearchTextEdited:   (t) => controller.searchText = t
                onCategoryEdited:     (t) => controller.categoryFilter = t
                onSearchRequested:    () => { /* TODO: trigger controller search */ }
                onPptPathEdited:      (p) => controller.pptPath = p
                onPptSearchRequested: ()  => controller.reloadPpt()
            }
        }
    }
}
