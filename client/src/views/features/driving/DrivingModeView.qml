import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

//行车模式窗口（独立窗口，显示单画面实时视频）
Window {
    id: drivingModeWindow
    width: 960
    height: 600
    title: "行车模式 - 360度智能行车辅助系统"
    color: "black"

    property var settingsViewRef: null

    // 录制状态
    property bool isRecording: videoRecorderService ? videoRecorderService.isRecording : false
    property string recordStartTime: ""
    // 录制时长直接使用C++后端的recordDuration（避免QML计时器与C++不同步）
    property int recordDurationSec: videoRecorderService ? videoRecorderService.recordDuration : 0

    // 菜单栏显示/隐藏状态
    property bool menuVisible: true

    // 红绿灯检测状态 - 绑定C++后端OpenCV检测结果
    // 状态值: "red"/"green"/"yellow"/"detected"(检测到但颜色未确认)/"unknown"
    property string trafficLightState: (videoRecorderService && videoRecorderService.isTrafficLightDetecting) ? videoRecorderService.trafficLightState : "unknown"

    // 红绿灯颜色（detected按未知处理）
    property color trafficLightBgColor: trafficLightState === "red" ? "#cc0000" : trafficLightState === "green" ? "#00aa00" : trafficLightState === "yellow" ? "#ccaa00" : "#555555"
    property color trafficLightFgColor: trafficLightState === "red" ? "#ff3333" : trafficLightState === "green" ? "#4CAF50" : trafficLightState === "yellow" ? "#FFC107" : "#aaaaaa"
    property string trafficLightLabelText: trafficLightState === "red" ? "红灯 - 停车" : trafficLightState === "green" ? "绿灯 - 通行" : trafficLightState === "yellow" ? "黄灯 - 减速" : trafficLightState === "detected" ? "检测中..." : ""
    property string trafficLightStatusText: trafficLightState === "red" ? "红灯" : trafficLightState === "green" ? "绿灯" : trafficLightState === "yellow" ? "黄灯" : trafficLightState === "detected" ? "检测中" : "检测中"

    // 截图计数
    property int screenshotCount: screenshotViewModel ? screenshotViewModel.uploadCount : 0

    // 连接VideoRecorderService截图信号到ScreenshotViewModel
    Connections {
        target: videoRecorderService
        onScreenshotSaved: {
            if (screenshotViewModel) {
                screenshotViewModel.uploadScreenshot(filePath, detectionInfo, "行车截图")
            }
        }
        // 录制完成后上传视频记录到服务器
        onRecordingSaved: {
            if (videoRecordViewModel) {
                var fileName = filePath.split('/').pop()
                var resolution = width + "x" + height
                videoRecordViewModel.uploadVideoRecord(
                    fileName,       // videoName
                    fileSizeMB,     // videoSizeMB
                    filePath,       // videoPath
                    "行车记录",     // recordType
                    durationSec,    // durationSec
                    resolution,     // resolution
                    fps,            // fps
                    "前摄像头"      // cameraSource
                )
            }
        }
    }

    // 格式化录制时长
    function formatRecordDuration(sec) {
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        var s = sec % 60
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // 获取当前日期时间字符串（用于文件名）
    function getDateTimeStr() {
        var d = new Date()
        var year = d.getFullYear()
        var month = (d.getMonth() + 1) < 10 ? "0" + (d.getMonth() + 1) : (d.getMonth() + 1)
        var day = d.getDate() < 10 ? "0" + d.getDate() : d.getDate()
        var hour = d.getHours() < 10 ? "0" + d.getHours() : d.getHours()
        var min = d.getMinutes() < 10 ? "0" + d.getMinutes() : d.getMinutes()
        var sec = d.getSeconds() < 10 ? "0" + d.getSeconds() : d.getSeconds()
        return year + month + day + "_" + hour + min + sec
    }

    // 窗口加载完成后打开摄像头
    Component.onCompleted: {
        if (videoRecorderService) {
            videoRecorderService.openCamera(0)
        }
    }

    onClosing: {
        if (settingsViewRef) {
            settingsViewRef.activeButtonIndex = 5
        }
        // 关闭摄像头（停止录制会自动在closeCamera中处理）
        if (videoRecorderService) {
            videoRecorderService.closeCamera()
        }
    }

    // 主布局：左侧菜单 + 右侧视频区域
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧菜单栏（可隐藏）
        Rectangle {
            id: leftMenu
            Layout.preferredWidth: menuVisible ? 180 : 0
            Layout.fillHeight: true
            color: "#1a1a1a"
            clip: true

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 300 }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                visible: menuVisible

                Text {
                    text: "行车模式"
                    color: "#4CAF50"
                    font.pointSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 录制控制
                Text {
                    text: "录制控制"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Button {
                    id: recordBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 45
                    text: isRecording ? "停止录制" : "开始录制"
                    font.pointSize: 14

                    background: Rectangle {
                        color: isRecording ? "#cc3333" : (recordBtn.hovered ? "#388E3C" : "#4CAF50")
                        radius: 6
                    }

                    contentItem: Text {
                        text: recordBtn.text
                        color: "white"
                        font.pointSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (isRecording) {
                            if (videoRecorderService) {
                                videoRecorderService.stopRecording()
                            }
                        } else {
                            if (videoRecorderService && videoRecorderService.startRecording()) {
                                recordStartTime = getDateTimeStr()
                            }
                        }
                    }
                }

                // 录制时长显示
                Rectangle {
                    visible: isRecording
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#330000"
                    radius: 4

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5

                        Rectangle {
                            width: 8
                            height: 8
                            color: "#ff3333"
                            radius: 4

                            SequentialAnimation on opacity {
                                running: isRecording
                                loops: Animation.Infinite
                                NumberAnimation { from: 1; to: 0.3; duration: 500 }
                                NumberAnimation { from: 0.3; to: 1; duration: 500 }
                            }
                        }

                        Text {
                            text: "REC " + formatRecordDuration(recordDurationSec)
                            color: "#ff3333"
                            font.pointSize: 13
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 车辆识别开关
                Text {
                    text: "车辆识别"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Switch {
                    id: vehicleDetectionSwitch
                    checked: true
                    Layout.fillWidth: true

                    indicator: Rectangle {
                        x: vehicleDetectionSwitch.leftPadding
                        y: parent.height / 2 - height / 2
                        width: 36
                        height: 20
                        radius: 10
                        color: vehicleDetectionSwitch.checked ? "#4CAF50" : "#555555"

                        Rectangle {
                            x: vehicleDetectionSwitch.checked ? parent.width - width - 3 : 3
                            y: parent.height / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: "white"
                        }
                    }

                    onCheckedChanged: {
                        if (videoRecorderService) {
                            videoRecorderService.isDetecting = checked
                        }
                    }
                }

                // 红绿灯检测开关
                Text {
                    text: "红绿灯检测"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Switch {
                    id: trafficLightSwitch
                    checked: true
                    Layout.fillWidth: true

                    indicator: Rectangle {
                        x: trafficLightSwitch.leftPadding
                        y: parent.height / 2 - height / 2
                        width: 36
                        height: 20
                        radius: 10
                        color: trafficLightSwitch.checked ? "#4CAF50" : "#555555"

                        Rectangle {
                            x: trafficLightSwitch.checked ? parent.width - width - 3 : 3
                            y: parent.height / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: "white"
                        }
                    }

                    onCheckedChanged: {
                        if (videoRecorderService) {
                            videoRecorderService.isTrafficLightDetecting = checked
                        }
                        if (!checked) {
                            trafficLightState = "unknown"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 自动截图
                Text {
                    text: "自动截图"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Switch {
                    id: autoScreenshotSwitch
                    checked: false
                    Layout.fillWidth: true

                    indicator: Rectangle {
                        x: autoScreenshotSwitch.leftPadding
                        y: parent.height / 2 - height / 2
                        width: 36
                        height: 20
                        radius: 10
                        color: autoScreenshotSwitch.checked ? "#2196F3" : "#555555"

                        Rectangle {
                            x: autoScreenshotSwitch.checked ? parent.width - width - 3 : 3
                            y: parent.height / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: "white"
                        }
                    }

                    onCheckedChanged: {
                        if (videoRecorderService) {
                            videoRecorderService.isAutoScreenshotEnabled = checked
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        text: "间隔："
                        color: "#cccccc"
                        font.pointSize: 11
                    }

                    SpinBox {
                        id: screenshotIntervalSpinBox
                        from: 5
                        to: 120
                        value: 15
                        editable: true
                        Layout.fillWidth: true

                        textFromValue: function(value) {
                            return value + "秒"
                        }

                        valueFromText: function(text) {
                            return parseInt(text)
                        }

                        onValueChanged: {
                            if (videoRecorderService) {
                                videoRecorderService.autoScreenshotIntervalSec = value
                            }
                        }

                        background: Rectangle {
                            color: "#333333"
                            radius: 4
                        }
                    }
                }

                // 截图计数
                Rectangle {
                    visible: autoScreenshotSwitch.checked
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: "#1a2a3a"
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "已上传截图：" + screenshotCount
                        color: "#2196F3"
                        font.pointSize: 11
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 手动截图按钮
                Button {
                    id: manualScreenshotBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: "📸 手动截图"
                    font.pointSize: 13

                    background: Rectangle {
                        color: manualScreenshotBtn.hovered ? "#1976D2" : "#1565C0"
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (videoRecorderService) {
                            var path = videoRecorderService.takeManualScreenshot()
                            if (path !== "") {
                                screenshotCount++
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // 全屏按钮
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: drivingModeWindow.visibility === Window.FullScreen ? "退出全屏" : "全屏"
                    font.pointSize: 12

                    background: Rectangle {
                        color: parent.hovered ? "#444444" : "#333333"
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (drivingModeWindow.visibility === Window.FullScreen) {
                            drivingModeWindow.showNormal()
                        } else {
                            drivingModeWindow.showFullScreen()
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "关闭"
                    font.pointSize: 12

                    background: Rectangle {
                        color: parent.hovered ? "#cc3333" : "#993333"
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: drivingModeWindow.close()
                }
            }
        }

        // 右侧视频显示区域（单画面）
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1a1a1a"

            // 菜单隐藏/显示切换按钮
            Button {
                id: menuToggleBtn
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 8
                width: 36
                height: 36
                z: 10

                background: Rectangle {
                    color: menuToggleBtn.hovered ? "#555555" : "#333333"
                    radius: 4
                    opacity: 0.8
                }

                contentItem: Text {
                    text: menuVisible ? "\u25C0" : "\u25B6"
                    color: "white"
                    font.pointSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    menuVisible = !menuVisible
                }
            }

            // 单画面实时视频
            Image {
                id: liveCameraImage
                anchors.fill: parent
                source: (videoRecorderService && videoRecorderService.isCameraOpen)
                        ? "image://videoframe/live?" + videoRecorderService.frameCounter : ""
                fillMode: Image.PreserveAspectFit
                cache: false
                asynchronous: false

                // 无画面时显示占位文字
                Text {
                    anchors.centerIn: parent
                    text: liveCameraImage.status !== Image.Ready ? "等待摄像头..." : ""
                    color: "#888888"
                    font.pointSize: 18
                }
            }

            // 水印 - 左下角（企业logo或个人签名）
            RowLayout {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 8
                spacing: 4
                z: 5

                Rectangle {
                    width: 20
                    height: 20
                    color: "#4CAF50"
                    radius: 3

                    Text {
                        anchors.centerIn: parent
                        text: "360"
                        color: "white"
                        font.pointSize: 6
                        font.bold: true
                    }
                }

                Text {
                    text: "360°行车辅助"
                    color: Qt.rgba(1, 1, 1, 0.5)
                    font.pointSize: 10
                    font.italic: true
                }
            }

            // 红绿灯检测文字标注（右上角显示）
            Rectangle {
                visible: trafficLightSwitch.checked && trafficLightState !== "unknown"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                width: trafficLightText.width + 40
                height: 40
                color: trafficLightBgColor
                radius: 8
                opacity: 0.9
                z: 5

                // 灯光指示圆点
                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 10
                    width: 18
                    height: 18
                    radius: 9
                    color: "white"
                    opacity: 0.9

                    SequentialAnimation on opacity {
                        running: trafficLightState !== "unknown"
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.4; duration: 800 }
                        NumberAnimation { from: 0.4; to: 1.0; duration: 800 }
                    }
                }

                Text {
                    id: trafficLightText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 10
                    text: trafficLightLabelText
                    color: "white"
                    font.pointSize: 13
                    font.bold: true
                }
            }

            // 录制中指示器（画面上方浮动）
            Rectangle {
                visible: isRecording
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 12
                width: recIndicator.width + 20
                height: 30
                color: "#990000"
                radius: 15
                opacity: 0.85
                z: 5

                RowLayout {
                    id: recIndicator
                    anchors.centerIn: parent
                    spacing: 5

                    Rectangle {
                        width: 8
                        height: 8
                        color: "#ff3333"
                        radius: 4

                        SequentialAnimation on opacity {
                            running: isRecording
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 0.2; duration: 600 }
                            NumberAnimation { from: 0.2; to: 1; duration: 600 }
                        }
                    }

                    Text {
                        text: "录制中 " + formatRecordDuration(recordDurationSec)
                        color: "white"
                        font.pointSize: 12
                        font.bold: true
                    }
                }
            }
        }
    }

    // 底部状态栏
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: "#111111"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 15

            Text {
                text: videoRecorderService && videoRecorderService.isCameraOpen
                      ? "状态：摄像头已连接" : "状态：摄像头未连接"
                color: videoRecorderService && videoRecorderService.isCameraOpen ? "#4CAF50" : "#ff6b6b"
                font.pointSize: 11
            }

            Text {
                text: videoRecorderService
                      ? "帧率：" + videoRecorderService.cameraFps + "fps" : "帧率：--"
                color: "#aaaaaa"
                font.pointSize: 11
            }

            Text {
                text: videoRecorderService
                      ? "分辨率：" + videoRecorderService.cameraWidth + "x" + videoRecorderService.cameraHeight
                      : "分辨率：--"
                color: "#aaaaaa"
                font.pointSize: 11
            }

            Text {
                text: "车辆识别：" + (vehicleDetectionSwitch.checked ? "已开启" : "已关闭")
                color: vehicleDetectionSwitch.checked ? "#4CAF50" : "#aaaaaa"
                font.pointSize: 11
            }

            Text {
                text: "红绿灯：" + (trafficLightSwitch.checked ? trafficLightStatusText : "关闭")
                color: trafficLightFgColor
                font.pointSize: 11
            }

            Text {
                text: "截图：" + (autoScreenshotSwitch.checked ? "每" + screenshotIntervalSpinBox.value + "秒" : "关闭")
                color: autoScreenshotSwitch.checked ? "#2196F3" : "#aaaaaa"
                font.pointSize: 11
                Layout.alignment: Qt.AlignRight
            }
        }
    }
}
