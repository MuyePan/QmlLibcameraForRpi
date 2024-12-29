import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QmlLibcamera

Window {
    width: 800
    height: 530
    visible: true
    title: qsTr("QLibCamera Test")

    Rectangle {
        anchors.fill: parent
        border.color: 'black'
    }

    LibCamera {
        id: camera
        view: cameraView
        index: 0                            // camera index, default 0
        width: 640                          // default 640
        height: 480                         // default 480
        format: LibCamera.Format_RGB888     // default Format_RGB565
        fps: 10                             // default 15
        enabled: false                      // default false
        recordBitRate: 400000               // default 300000


        onSnapshotCompleted: (filename) => {
                                 autoHideCompleteStatus.running = true
                                 completeStatus.text = "Image has been saved to " + filename
                                 completeStatus.visible = true
        }

        onRecordingCompleted: (filename, frameTotal) => {
                                 autoHideCompleteStatus.running = true
                                 completeStatus.text = "Video has been saved to " + filename + "\n" + "Total Frame:" + frameTotal
                                 completeStatus.visible = true
                             }
    }

    LibCameraView {
        id: cameraView
        width: camera.width
        height: camera.height

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 10
        anchors.leftMargin: 10

        refreshRateLimit: 10                           // default 15

        Rectangle {
            anchors.fill: parent
            border.color: 'black'
            visible: !camera.enabled

            Label {
                text: "No Camera Stream"
                color: "red"

                anchors.centerIn: parent
                font.pixelSize: 20
            }
        }

        Label {
            id: completeStatus

            color: "red"

            font.pixelSize: 20
        }
    }

    Rectangle {
        anchors.left : cameraView.left
        anchors.right: cameraView.right
        anchors.top: cameraView.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 5
        anchors.bottomMargin: 5

        border.color: 'black'

        Label {
            id: status

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 5
        }
    }

    ColumnLayout {
        anchors.left: cameraView.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10

        Label {
            id: fps
            lineHeight: 2

            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: !camera.enabled ? "Start Camera" : "Stop Camera"
            implicitWidth: 120
            implicitHeight: 40

            Layout.alignment: Qt.AlignHCenter

            onClicked: camera.enabled = !camera.enabled
        }

        Button {
            text: "Snapshot"
            implicitWidth: 120
            implicitHeight: 40

            Layout.alignment: Qt.AlignHCenter

            onClicked: camera.snapshot()
        }

        Button {
            text: camera.isRecording ? "End Recording" : "Start Recording"
            implicitWidth: 120
            implicitHeight: 40

            Layout.alignment: Qt.AlignHCenter

            onClicked: camera.isRecording ? camera.endRecording() : camera.startRecording()
        }
    }

    Timer {
        id: autoHideCompleteStatus

        interval: 5000
        running: false
        repeat: false
        triggeredOnStart: false

        onTriggered: {
            completeStatus.visible = false
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            fps.text = "FPS:" + (camera.enabled ? camera.curFps.toFixed(2) : "0.00")
            if(camera.isRecording) {
                status.text = "Frame Recorded:" + camera.framesRecorded
            }
            else if(camera.enabled) {
                status.text = "Frame Captured:" + camera.framesCaptured
            }
            else {
                status.text = "Idle"
            }
        }
    }
}
