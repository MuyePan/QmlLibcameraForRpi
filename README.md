# Using Qt Quick/QML with libcamera on Raspberry Pi
This page shows how to use QmlLibcamera. 

It is highly recommended to watch video tutorial as there is additional content in the video tutorial.

Click the following image to view this tutorial on Youtube.

[![Youtube video link](https://i.ytimg.com/vi/gTd6dm9ONSk/hqdefault.jpg)](//youtu.be/gTd6dm9ONSk "Youtube Video")

- Cross Compilation https://youtu.be/8kpHgNKPooc
- Remote Debugging https://youtu.be/QWz-4R4kMIo

## Using QML
main.qml
```
    LibCamera {
      id: camera
      view: cameraView
      index: 0                            // camera index, default 0
      width: 640                          // default 640
      height: 480                         // default 480
      format: LibCamera.Format_RGB888     // default Format_RGB565
      fps: 10                             // default 15
      enabled: true                       // default false
      recordBitRate: 400000               // default 300000
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
  }
```

## Place to write processing code like working with openCV
**qlibcamera/qlibcameraworker.cpp**
```
void LibCameraProcessWorker::process()
{
    // TODO: DO YOUR PROCESSINGS HERE
    // const uchar * data = image_.bits()
}
```
  

  
  
