import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import GarmentStyleMatch

Rectangle {
    id: root

    property var styleGalleryModel
    property var pagesModel
    property int pptSelectedCount: 0
    property bool busy: false
    property string categoryText: qsTr("全部")
    property string searchText: ""
    property string pptPath: ""
    property bool currentPhotoSelected: false
    property var categoryRuleOptions: []
    property string currentCategoryRule: ""
    property string categoryRuleStatus: ""
    property string categorySummary: ""
    property bool startupTabInitialized: false
    readonly property bool showPptTab: Qt.platform.os === "windows"

    signal searchTextEdited(string text)
    signal categoryEdited(string text)
    signal categoryRuleSelected(string ruleId)
    signal reloadCategoryRuleRequested()
    signal pptPathEdited(string path)
    signal pptSearchRequested()
    signal pptPageToggled(int row)
    signal extractRequested()
    signal galleryMatchRequested(int galleryRow, string part)
    signal galleryConfirmRequested(int galleryRow, string part)

    function initializeStartupTab(loadFinished) {
        if (startupTabInitialized)
            return

        if (!showPptTab) {
            tabBar.currentIndex = 1
            startupTabInitialized = true
            return
        }

        if (!styleGalleryModel
                || (!loadFinished && styleGalleryModel.count === 0))
            return

        tabBar.currentIndex = styleGalleryModel.count > 0 ? 1 : 0
        startupTabInitialized = true
    }

    Component.onCompleted: initializeStartupTab(false)

    Connections {
        target: root.styleGalleryModel
        function onCountChanged() {
            root.initializeStartupTab(true)
        }
    }

    color: Theme.background
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton {
                text: qsTr("PPT页面预览")
                visible: root.showPptTab
                width: visible ? implicitWidth : 0
            }
            TabButton { text: qsTr("款号小图库") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            PptPreviewPanel {
                model:          root.pagesModel
                pptPath:        root.pptPath
                selectedCount:  root.pptSelectedCount
                busy:           root.busy
                onPptPathEdited:      (p) => root.pptPathEdited(p)
                onPptSearchRequested: ()  => root.pptSearchRequested()
                onPageToggled:        (r) => root.pptPageToggled(r)
                onExtractRequested:   ()  => {
                    root.extractRequested()
                    tabBar.currentIndex = 1
                }
            }

            StyleGalleryPanel {
                model:         root.styleGalleryModel
                categoryText:  root.categoryText
                searchText:    root.searchText
                uiStyle:       controller.currentUiStyle
                currentPhotoSelected: root.currentPhotoSelected
                categoryRuleOptions: root.categoryRuleOptions
                currentCategoryRule: root.currentCategoryRule
                categoryRuleStatus: root.categoryRuleStatus
                categorySummary: root.categorySummary
                onSearchTextEdited: (t) => root.searchTextEdited(t)
                onCategoryEdited:   (t) => root.categoryEdited(t)
                onCategoryRuleSelected: (ruleId) => root.categoryRuleSelected(ruleId)
                onReloadCategoryRuleRequested: root.reloadCategoryRuleRequested()
                onMatchRequested: (row, part) => root.galleryMatchRequested(row, part)
                onConfirmRequested: (row, part) => root.galleryConfirmRequested(row, part)
            }
        }
    }
}
