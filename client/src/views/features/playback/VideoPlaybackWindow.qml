import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//视频播放独立窗口 - 场景重现（OpenCV本地播放+检测+截图上传）
Window {
    id: playbackWindow
    width: 960
    height: 640
    title: "场景重现 - " + videoTitle
    color: "#1a1a1a"

    // 外部传入属性
    property string videoTitle: ""
    property string videoFilePath: ""
    property int videoDurationSec: 0
    property int resumePositionSec: 0
    property int videoFps: 30
    property int videoId: 0

    // 播放状态（从ViewModel获取）- 使用毫秒级时间（更可靠）
    property bool isPlaying: videoPlaybackViewModel ? videoPlaybackViewModel.isPlaying : false
    property int curPosMs: videoPlaybackViewModel ? videoPlaybackViewModel.currentPositionMs : 0
    property int curPosSec: curPosMs > 0 ? Math.floor(curPosMs / 1000) : 0
    property int totalDurationMs: {
        if (!videoPlaybackViewModel) return videoDurationSec * 1000
        var vmMs = videoPlaybackViewModel.totalDurationMs
        return vmMs > 0 ? vmMs : videoDurationSec * 1000
    }
    property int totalDuration: totalDurationMs > 0 ? Math.ceil(totalDurationMs / 1000) : videoDurationSec
    property double playbackSpeed: 1.0
    property int currentFrame: videoPlaybackViewModel ? videoPlaybackViewModel.currentFrame : 0
    property int totalFrames: videoPlaybackViewModel ? videoPlaybackViewModel.totalFrames : 0

    // 特征检测状态
    property bool vehicleDetectEnabled: true
    property bool trafficLightDetectEnabled: true

    // 当前帧图像
    property alias currentFrameImage: frameImage.source

    // 格式化时间 mm:ss
    function formatTime(sec) {
        var m = Math.floor(sec / 60)
        var s = Math.floor(sec % 60)
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // 续播提示
    Component.onCompleted: {
        if (videoPlaybackViewModel && videoFilePath !== "") {
            videoPlaybackViewModel.openVideo(videoFilePath, resumePositionSec)
            // 打开视频后自动开始播放
            videoPlaybackViewModel.togglePlayPause()
            if (resumePositionSec > 0) {
                resumeBanner.visible = true
                resumeBannerTimer.start()
            }
        }
    }

    // 关闭时保存播放位置
    onClosing: {
        if (videoPlaybackViewModel && videoId > 0 && curPosSec > 0) {
            videoPlaybackViewModel.updatePlayRecord(videoId, curPosSec)
        }
        if (videoPlaybackViewModel) {
            videoPlaybackViewModel.closeVideo()
        }
    }

    // 续播提示自动隐藏
    Timer {
        id: resumeBannerTimer
        interval: 3000
        onTriggered: resumeBanner.visible = false
    }

    // 连接ViewModel的新帧信号
    Connections {
        target: videoPlaybackViewModel
        onNewFrameReady: {
            // 将QImage转换为data URL供Image组件显示
            // 使用临时文件方式
        }
        onScreenshotUploaded: {
            screenshotDialog.title = success ? "截图上传成功" : "截图上传失败"
            screenshotDialog.text = message
            screenshotDialog.visible = true
        }
    }

    // 截图成功/失败提示弹窗
    MessageDialog {
        id: screenshotDialog
        modality: Qt.NonModal
        standardButtons: StandardButton.Ok
    }

    // 视频显示区域
    Rectangle {
        id: videoDisplayArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controlBar.top
        color: "#000000"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            color: "#2a2a2a"

            // 视频帧显示（通过ImageProvider高效显示）
            Image {
                id: frameImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                cache: false
                source: (videoPlaybackViewModel && videoPlaybackViewModel.videoLoaded)
                        ? "image://videoframe/playback?" + videoPlaybackViewModel.frameCounter : ""
                visible: source !== ""
            }

            // 当无视频帧时的占位文字
            Text {
                anchors.centerIn: parent
                text: videoFilePath !== "" ? videoTitle : "请选择视频文件播放"
                color: "#888888"
                font.pointSize: 20
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                visible: !frameImage.visible
            }

            // 视频信息
            Text {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 15
                text: videoPlaybackViewModel && videoPlaybackViewModel.videoLoaded ?
                      "分辨率: " + videoPlaybackViewModel.fps + "fps | 总帧数: " + totalFrames : ""
                color: "#aaaaaa"
                font.pointSize: 12
            }
        }

        // 续播提示横幅
        Rectangle {
            id: resumeBanner
            visible: false
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 20
            width: resumeBannerText.width + 30
            height: 36
            color: "#1976D2"
            radius: 18
            opacity: 0.9

            Text {
                id: resumeBannerText
                anchors.centerIn: parent
                text: "\u4ECE\u4E0A\u6B21\u64AD\u653E\u4F4D\u7F6E\u7EE7\u7EED"
                color: "white"
                font.pointSize: 13
            }
        }

        // 暂停状态大图标
        Text {
            visible: !isPlaying && totalFrames > 0
            anchors.centerIn: parent
            text: "\u25B6"
            color: "#ffffff"
            font.pointSize: 60
            opacity: 0.6

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (videoPlaybackViewModel) videoPlaybackViewModel.togglePlayPause()
                }
            }
        }

        // 水印 - 左下角显示
        RowLayout {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 6

            Rectangle {
                width: 24
                height: 24
                color: "#4CAF50"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: "360"
                    color: "white"
                    font.pointSize: 7
                    font.bold: true
                }
            }

            Text {
                text: "360\u00B0\u667A\u80FD\u884C\u8F66\u8F85\u52A9"
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pointSize: 12
                font.italic: true
            }
        }

        // 鼠标悬停显示控制栏
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onContainsMouseChanged: {
                controlBar.opacity = containsMouse ? 1.0 : 0.3
            }
        }
    }

    // 底部控制栏
    Rectangle {
        id: controlBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 90
        color: "#1a1a1a"
        opacity: 1.0

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.topMargin: 5
            spacing: 4

            // 进度条区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: formatTime(curPosSec)
                    color: "#ffffff"
                    font.pointSize: 12
                    Layout.preferredWidth: 50
                    horizontalAlignment: Text.AlignRight
                }

                // 播放进度条（可拖拽）- 使用毫秒精度避免MJPEG帧数虚高问题
                Slider {
                    id: progressSlider
                    Layout.fillWidth: true
                    from: 0
                    to: totalDurationMs > 0 ? totalDurationMs : 1
                    value: 0
                    live: false
                    property bool sliding: false

                    onPressedChanged: {
                        if (pressed) {
                            sliding = true
                        } else {
                            sliding = false
                            if (videoPlaybackViewModel) {
                                videoPlaybackViewModel.seekToSec(value / 1000.0)
                            }
                        }
                    }

                    // 播放位置更新（不拖拽时跟随）
                    Connections {
                        target: playbackWindow
                        onCurPosMsChanged: {
                            if (!progressSlider.sliding && !progressSlider.pressed) {
                                progressSlider.value = curPosMs
                            }
                        }
                    }

                    background: Rectangle {
                        x: progressSlider.leftPadding
                        y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                        width: progressSlider.availableWidth
                        height: 4
                        radius: 2
                        color: "#555555"

                        Rectangle {
                            width: progressSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#4CAF50"
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: progressSlider.leftPadding + progressSlider.visualPosition * (progressSlider.availableWidth - width)
                        y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: progressSlider.pressed ? "#ffffff" : "#4CAF50"
                        border.color: "#ffffff"
                        border.width: 1
                    }
                }

                Text {
                    text: formatTime(totalDuration)
                    color: "#aaaaaa"
                    font.pointSize: 12
                    Layout.preferredWidth: 50
                }
            }

            // 控制按钮行
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // 播放/暂停按钮
                Button {
                    id: playPauseBtn
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    text: isPlaying ? "\u23F8" : "\u25B6"

                    background: Rectangle {
                        color: playPauseBtn.hovered ? "#444444" : "#333333"
                        radius: 6
                    }

                    contentItem: Text {
                        text: playPauseBtn.text
                        color: "white"
                        font.pointSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (videoPlaybackViewModel) videoPlaybackViewModel.togglePlayPause()
                    }
                }

                // 停止按钮
                Button {
                    id: stopBtn
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    text: "\u23F9"

                    background: Rectangle {
                        color: stopBtn.hovered ? "#444444" : "#333333"
                        radius: 6
                    }

                    contentItem: Text {
                        text: stopBtn.text
                        color: "white"
                        font.pointSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (videoPlaybackViewModel) {
                            videoPlaybackViewModel.seekToSec(0)
                            videoPlaybackViewModel.closeVideo()
                            videoPlaybackViewModel.openVideo(videoFilePath)
                        }
                    }
                }

                // 帧数显示
                Rectangle {
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 36
                    color: "#333333"
                    radius: 6

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            text: "\u5E27:"
                            color: "#aaaaaa"
                            font.pointSize: 11
                        }
                        Text {
                            text: currentFrame
                            color: "#4CAF50"
                            font.pointSize: 13
                            font.bold: true
                        }
                        Text {
                            text: "/" + totalFrames
                            color: "#888888"
                            font.pointSize: 11
                        }
                    }
                }

                // 特征检测开关
                RowLayout {
                    spacing: 6

                    Text {
                        text: "\u68C0\u6D4B:"
                        color: "#aaaaaa"
                        font.pointSize: 11
                    }

                    Button {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 28
                        text: "\u8F66"

                        background: Rectangle {
                            color: vehicleDetectEnabled ? "#4CAF50" : "#555555"
                            radius: 4
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pointSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            vehicleDetectEnabled = !vehicleDetectEnabled
                            if (videoPlaybackViewModel) videoPlaybackViewModel.vehicleDetectEnabled = vehicleDetectEnabled
                        }
                    }

                    Button {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 28
                        text: "\u706F"

                        background: Rectangle {
                            color: trafficLightDetectEnabled ? "#FF9800" : "#555555"
                            radius: 4
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pointSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            trafficLightDetectEnabled = !trafficLightDetectEnabled
                            if (videoPlaybackViewModel) videoPlaybackViewModel.trafficLightDetectEnabled = trafficLightDetectEnabled
                        }
                    }
                }

                // 截图+上传按钮
                Button {
                    id: screenshotBtn
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 36
                    text: "\uD83D\uDCF7 \u622A\u56FE\u4E0A\u4F20"

                    background: Rectangle {
                        color: screenshotBtn.hovered ? "#1976D2" : "#1565C0"
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (videoPlaybackViewModel) {
                            videoPlaybackViewModel.takeScreenshotAndUpload()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 倍速播放
                RowLayout {
                    spacing: 4

                    Text {
                        text: "\u500D\u901F:"
                        color: "#aaaaaa"
                        font.pointSize: 12
                    }

                    Repeater {
                        model: [
                            { speed: 0.5, label: "0.5x" },
                            { speed: 1.0, label: "1x" },
                            { speed: 1.5, label: "1.5x" },
                            { speed: 2.0, label: "2x" }
                        ]

                        Button {
                            Layout.preferredWidth: 45
                            Layout.preferredHeight: 30
                            text: modelData.label
                            font.pointSize: 11

                            background: Rectangle {
                                color: playbackSpeed === modelData.speed ? "#4CAF50" :
                                       (hovered ? "#444444" : "#333333")
                                radius: 4
                            }

                            contentItem: Text {
                                text: parent.text
                                color: playbackSpeed === modelData.speed ? "white" : "#cccccc"
                                font.pointSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                playbackSpeed = modelData.speed
                                if (videoPlaybackViewModel) videoPlaybackViewModel.playbackSpeed = modelData.speed
                            }
                        }
                    }
                }

                // 全屏按钮
                Button {
                    id: fullscreenBtn
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    text: "\u26F6"

                    background: Rectangle {
                        color: fullscreenBtn.hovered ? "#444444" : "#333333"
                        radius: 6
                    }

                    contentItem: Text {
                        text: fullscreenBtn.text
                        color: "white"
                        font.pointSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (playbackWindow.visibility === Window.FullScreen) {
                            playbackWindow.showNormal()
                        } else {
                            playbackWindow.showFullScreen()
                        }
                    }
                }

                // 关闭按钮
                Button {
                    id: closeWindowBtn
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 36
                    text: "\u5173\u95ED"

                    background: Rectangle {
                        color: closeWindowBtn.hovered ? "#cc3333" : "#993333"
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        playbackWindow.close()
                    }
                }
            }
        }
    }
}
