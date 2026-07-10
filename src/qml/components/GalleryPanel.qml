import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var galleryModel
    property var pagesModel
    property int pptSelectedCount: 0
    property string categoryText: qsTr("全部")
    property string searchText: ""
    property string pptPath: ""

    signal searchTextEdited(string text)
    signal categoryEdited(string text)
    signal searchRequested()
    signal pptPathEdited(string path)
    signal pptSearchRequested()
    signal pptPageToggled(int row)
    signal extractRequested()

    color: "#f5f7fa"
    border.color: "#dee3e8"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton { text: qsTr("PPT页面预览") }
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
                onPptPathEdited:      (p) => root.pptPathEdited(p)
                onPptSearchRequested: ()  => root.pptSearchRequested()
                onPageToggled:        (r) => root.pptPageToggled(r)
                onExtractRequested:   ()  => {
                    root.extractRequested()
                    tabBar.currentIndex = 1
                }
            }

            StyleGalleryPanel {
                model:         root.galleryModel
                categoryText:  root.categoryText
                searchText:    root.searchText
                onSearchTextEdited: (t) => root.searchTextEdited(t)
                onCategoryEdited:   (t) => root.categoryEdited(t)
                onSearchRequested:  ()  => root.searchRequested()
            }
        }
    }
}
