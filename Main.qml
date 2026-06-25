import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ccvWriter

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    title: qsTr("Chat Live")

    CallSessionController {
        id: session
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        VideoPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: qsTr("本地预览")
            placeholder: qsTr("正在打开摄像头...")

            LocalVideoView {
                id: localView
                anchors.fill: parent
            }
        }

        VideoPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: qsTr("远端画面")
            placeholder: qsTr("等待远端视频...")

            RemoteVideoView {
                id: remoteView
                anchors.fill: parent
                placeholderText: qsTr("等待远端视频...")
            }
        }
    }

    Component.onCompleted: {
        session.bindRenderers(localView, remoteView)
        if (!session.startPreview()) {
            console.warn("Failed to start local preview")
        }
    }
}
