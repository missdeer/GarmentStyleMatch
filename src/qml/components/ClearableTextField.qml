import QtQuick
import QtQuick.Controls

TextField {
    id: root

    signal cleared()

    rightPadding: clearButton.width + 10
    Keys.onEscapePressed: (event) => {
        clearText()
        event.accepted = true
    }

    function clearText() {
        clear()
        cleared()
    }

    Rectangle {
        id: clearButton
        visible: root.text.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 16
        height: 16
        radius: width / 2
        color: clearArea.containsMouse ? "#8a99a8" : "#b0bcc7"

        Label {
            anchors.centerIn: parent
            text: "×"
            color: "white"
            font.pixelSize: 12
            font.bold: true
        }

        MouseArea {
            id: clearArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clearText()
        }
    }
}
