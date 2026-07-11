import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property var imagePaths: []
    property int currentIndex: -1

    signal imageActivated(int index)

    color: "#eef3f6"
    border.color: "#cfd8df"

    ToolButton {
        id: leftButton
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 24
        text: "\u2039"
        font.pixelSize: 20
        enabled: previewList.contentX > 0.5
        onClicked: previewList.scrollBy(-(72 + previewList.spacing))
        ToolTip.visible: hovered
        ToolTip.text: qsTr("向左滚动")
    }

    ToolButton {
        id: rightButton
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 24
        text: "\u203a"
        font.pixelSize: 20
        enabled: previewList.contentX < previewList.maximumContentX - 0.5
        onClicked: previewList.scrollBy(72 + previewList.spacing)
        ToolTip.visible: hovered
        ToolTip.text: qsTr("向右滚动")
    }

    ListView {
        id: previewList
        anchors.left: leftButton.right
        anchors.right: rightButton.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        orientation: ListView.Horizontal
        spacing: 8
        clip: true
        model: root.imagePaths
        currentIndex: root.currentIndex
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar {}
        readonly property real maximumContentX: Math.max(0, contentWidth - width)

        function scrollBy(distance) {
            contentX = Math.max(0, Math.min(maximumContentX, contentX + distance))
        }

        onCurrentIndexChanged: {
            if (currentIndex >= 0)
                positionViewAtIndex(currentIndex, ListView.Contain)
        }

        delegate: Rectangle {
            id: thumbnailCell
            required property int index
            required property string modelData

            width: 72
            height: previewList.height
            color: "#ffffff"
            border.width: index === previewList.currentIndex ? 2 : 1
            border.color: index === previewList.currentIndex ? "#2f73b7" : "#aeb9c2"

            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: thumbnailCell.modelData !== "" ? "file:///" + thumbnailCell.modelData : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.imageActivated(thumbnailCell.index)
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            z: 2

            onWheel: (wheel) => {
                let delta = wheel.pixelDelta.y
                if (delta === 0)
                    delta = wheel.pixelDelta.x
                if (delta === 0)
                    delta = wheel.angleDelta.y
                if (delta === 0)
                    delta = wheel.angleDelta.x
                previewList.scrollBy(-delta)
                wheel.accepted = true
            }
        }
    }
}
