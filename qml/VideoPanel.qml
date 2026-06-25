import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string label: ""
    property string placeholder: ""
    property bool showPlaceholder: videoContainer.children.length === 0
                                         || !videoContainer.children[0].hasFrame

    default property alias videoContent: videoContainer.data

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 8
    }

    Item {
        id: videoContainer
        anchors.fill: parent
        anchors.margins: 2
        clip: true
    }

    Column {
        anchors.centerIn: parent
        spacing: 8
        visible: root.showPlaceholder

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: "white"
            font.pixelSize: 16
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.placeholder
            color: "#888888"
            font.pixelSize: 14
        }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        text: root.label
        color: "white"
        padding: 4
        z: 1

        background: Rectangle {
            color: "#80000000"
            radius: 4
        }
    }
}
