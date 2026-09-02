import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// 倒车模式独立窗口 - 单摄像头+辅助线+障碍物检测
Window {
    id: reverseModeWindow
    width: 960
    height: 600
    title: "倒车模式 - 360度智能行车辅助系统"
    color: "black"

    property var settingsViewRef: null

    // 菜单可见状态
    property bool menuVisible: true

    // 录制状态
    property bool isAutoRecording: reverseRecordViewModel ? reverseRecordViewModel.isAutoRecording : false
    property int currentFrameCount: reverseRecordViewModel ? reverseRecordViewModel.currentFrameCount : 0
    property int targetFrames: reverseRecordViewModel ? reverseRecordViewModel.targetFrames : 300

    // 警告级别（从ReverseRecordService获取）
    property string warningLevel: reverseRecordService ? reverseRecordService.warningLevel : "safe"

    // 转向状态
    property real steeringAngle: reverseRecordService ? reverseRecordService.steeringAngle : 0
    property string steerDirection: steeringAngle < -0.1 ? "左" : steeringAngle > 0.1 ? "右" : "直"

    // 菜单自动隐藏定时器
    Timer {
        id: menuHideTimer
        interval: 3000
        onTriggered: {
            if (reverseModeWindow.activeFocusItem === null) {
                menuVisible = false
            }
        }
    }

    onClosing: {
        if (reverseRecordService) {
            reverseRecordService.stop()
            reverseRecordService.closeCamera()
        }
        // 关闭videoRecorderService的摄像头
        if (videoRecorderService) {
            videoRecorderService.closeCamera()
        }
        if (settingsViewRef) {
            settingsViewRef.activeButtonIndex = 5
        }
    }

    Component.onCompleted: {
        // 启动倒车服务（直接使用reverseRecordService，它连接了frameProvider）
        if (reverseRecordService) {
            // 优先加载静态倒车照片（无车环境下模拟倒车画面）
            reverseRecordService.loadStaticImage("/home/cccc/Pictures/360TestPicture/warning.png")
            reverseRecordService.start()
        }
        // 3秒后自动隐藏菜单
        menuHideTimer.start()
    }

    // 鼠标活动检测 - 移动鼠标时短暂显示菜单
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onMouseXChanged: {
            menuVisible = true
            menuHideTimer.restart()
        }
        onMouseYChanged: {
            menuVisible = true
            menuHideTimer.restart()
        }
    }

    // 主布局
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
                    text: "倒车模式"
                    color: "#E91E63"
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

                // 自动录制开关
                Text {
                    text: "自动录制"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Switch {
                    id: autoRecordSwitch
                    checked: false
                    Layout.fillWidth: true

                    indicator: Rectangle {
                        x: autoRecordSwitch.leftPadding
                        y: parent.height / 2 - height / 2
                        width: 36
                        height: 20
                        radius: 10
                        color: autoRecordSwitch.checked ? "#E91E63" : "#555555"

                        Rectangle {
                            x: autoRecordSwitch.checked ? parent.width - width - 3 : 3
                            y: parent.height / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: "white"
                        }
                    }

                    onCheckedChanged: {
                        if (reverseRecordViewModel) {
                            reverseRecordViewModel.isAutoRecordEnabled = checked
                        }
                    }
                }

                // 图片切换（有障碍/无障碍）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                Text {
                    text: "图片切换"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Button {
                    id: switchImageBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: reverseRecordService && reverseRecordService.useSecondImage ? "🔄 切换到有障碍" : "🔄 切换到无障碍"
                    font.pointSize: 11

                    background: Rectangle {
                        color: switchImageBtn.hovered ? "#1565C0" : "#1976D2"
                        radius: 4
                    }

                    contentItem: Text {
                        text: switchImageBtn.text
                        color: "white"
                        font.pointSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (reverseRecordService)
                            reverseRecordService.useSecondImage = !reverseRecordService.useSecondImage
                    }
                }

                Text {
                    text: reverseRecordService && reverseRecordService.useSecondImage ? "当前：无障碍图" : "当前：有障碍图"
                    color: reverseRecordService && reverseRecordService.useSecondImage ? "#4CAF50" : "#FF9800"
                    font.pointSize: 10
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                // 录制间隔
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        text: "间隔："
                        color: "#cccccc"
                        font.pointSize: 11
                    }

                    SpinBox {
                        id: intervalSpinBox
                        from: 5
                        to: 60
                        value: 10
                        editable: true
                        Layout.fillWidth: true

                        textFromValue: function(value) {
                            return value + "秒"
                        }

                        valueFromText: function(text) {
                            return parseInt(text)
                        }

                        onValueChanged: {
                            if (reverseRecordViewModel) {
                                reverseRecordViewModel.autoRecordIntervalSec = value
                            }
                        }

                        background: Rectangle {
                            color: "#333333"
                            radius: 4
                        }
                    }
                }

                // 目标帧数
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        text: "帧数："
                        color: "#cccccc"
                        font.pointSize: 11
                    }

                    SpinBox {
                        id: framesSpinBox
                        from: 100
                        to: 1000
                        stepSize: 50
                        value: 300
                        editable: true
                        Layout.fillWidth: true

                        onValueChanged: {
                            if (reverseRecordViewModel) {
                                reverseRecordViewModel.targetFrames = value
                            }
                        }

                        background: Rectangle {
                            color: "#333333"
                            radius: 4
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 转向控制
                Text {
                    text: "转向控制"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                // 方向指示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#222222"
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "方向：" + steerDirection
                        color: steerDirection === "左" ? "#4FC3F7" : steerDirection === "右" ? "#FF8A65" : "#81C784"
                        font.pointSize: 13
                        font.bold: true
                    }
                }

                // 三个方向按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Button {
                        id: steerLeftBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: "← 左转"
                        font.pointSize: 12

                        background: Rectangle {
                            color: steerLeftBtn.hovered ? "#1565C0" : "#1976D2"
                            radius: 4
                        }

                        contentItem: Text {
                            text: steerLeftBtn.text
                            color: "white"
                            font.pointSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (reverseRecordService) reverseRecordService.steerLeft()
                        }
                    }

                    Button {
                        id: steerCenterBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: "↑ 回正"
                        font.pointSize: 12

                        background: Rectangle {
                            color: steerCenterBtn.hovered ? "#2E7D32" : "#388E3C"
                            radius: 4
                        }

                        contentItem: Text {
                            text: steerCenterBtn.text
                            color: "white"
                            font.pointSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (reverseRecordService) reverseRecordService.steerCenter()
                        }
                    }

                    Button {
                        id: steerRightBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: "右转 →"
                        font.pointSize: 12

                        background: Rectangle {
                            color: steerRightBtn.hovered ? "#E65100" : "#F57C00"
                            radius: 4
                        }

                        contentItem: Text {
                            text: steerRightBtn.text
                            color: "white"
                            font.pointSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (reverseRecordService) reverseRecordService.steerRight()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 背景图片
                Text {
                    text: "背景图片"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "加载车尾照片"
                    font.pointSize: 11

                    background: Rectangle {
                        color: parent.hovered ? "#8E24AA" : "#7B1FA2"
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        // 重新加载倒车照片
                        if (reverseRecordService) {
                            reverseRecordService.loadStaticImage("/home/cccc/Pictures/360TestPicture/warning.png")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#444444"
                }

                // 视角设置
                Text {
                    text: "视角设置"
                    color: "#aaaaaa"
                    font.pointSize: 12
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "拼接画面"
                    font.pointSize: 12

                    background: Rectangle {
                        color: "#E91E63"
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pointSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillHeight: true }

                // 全屏按钮
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: reverseModeWindow.visibility === Window.FullScreen ? "退出全屏" : "全屏"
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
                        if (reverseModeWindow.visibility === Window.FullScreen) {
                            reverseModeWindow.showNormal()
                        } else {
                            reverseModeWindow.showFullScreen()
                        }
                    }
                }

                // 关闭按钮
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

                    onClicked: reverseModeWindow.close()
                }
            }
        }

        // 右侧视频显示区域（全屏显示倒车拼接画面）
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1a1a1a"

            // 菜单显示/隐藏切换按钮
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
                    if (menuVisible) {
                        menuHideTimer.restart()
                    }
                }
            }

            // 倒车拼接画面（占满整个区域）
            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                color: "#2a2a2a"
                radius: 4
                clip: true

                // 实时视频帧（使用reverseRecordService的帧，含辅助线叠加）
                Image {
                    id: reverseLiveImage
                    anchors.fill: parent
                    source: (reverseRecordService && reverseRecordService.isRunning)
                                    ? "image://videoframe/reverse?" + reverseRecordService.frameCounter : ""
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    asynchronous: false

                    // 无画面占位
                    Text {
                        anchors.centerIn: parent
                        text: reverseLiveImage.status !== Image.Ready ? "\u5012\u8F66\u8F85\u52A9\u753B\u9762" : ""
                        color: "#888888"
                        font.pointSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // 水印 - 左下角
                RowLayout {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    spacing: 6
                    z: 5

                    Rectangle {
                        width: 28
                        height: 28
                        color: "#E91E63"
                        radius: 5

                        Text {
                            anchors.centerIn: parent
                            text: "360"
                            color: "white"
                            font.pointSize: 8
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: "360\u00B0\u5012\u8F66\u8F85\u52A9"
                            color: Qt.rgba(1, 1, 1, 0.7)
                            font.pointSize: 13
                            font.italic: true
                        }

                        Text {
                            text: Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm:ss")
                            color: Qt.rgba(1, 1, 1, 0.4)
                            font.pointSize: 9
                        }
                    }
                }
            }

            // 自动录制指示器
            Rectangle {
                visible: isAutoRecording
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 12
                width: recIndicator.width + 20
                height: 36
                color: "#990033"
                radius: 18
                opacity: 0.85
                z: 10

                RowLayout {
                    id: recIndicator
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        width: 10
                        height: 10
                        color: "#ff3366"
                        radius: 5

                        SequentialAnimation on opacity {
                            running: isAutoRecording
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 0.2; duration: 500 }
                            NumberAnimation { from: 0.2; to: 1; duration: 500 }
                        }
                    }

                    Text {
                        text: "REC " + currentFrameCount + "/" + targetFrames
                        color: "#ff3366"
                        font.pointSize: 13
                        font.bold: true
                    }
                }
            }

            // WARNING 警告覆盖层（障碍物进入浅黄色区域时显示）
            Rectangle {
                visible: warningLevel === "warning" || warningLevel === "danger"
                anchors.centerIn: parent
                width: warningText.width + 60
                height: 80
                color: warningLevel === "danger" ? "#cc0000" : "#cc8800"
                radius: 12
                opacity: 0.85
                z: 20

                SequentialAnimation on opacity {
                    running: warningLevel === "warning" || warningLevel === "danger"
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.85; to: 0.3; duration: 500 }
                    NumberAnimation { from: 0.3; to: 0.85; duration: 500 }
                }

                Text {
                    id: warningText
                    anchors.centerIn: parent
                    text: warningLevel === "danger" ? "DANGER" : "WARNING"
                    color: "white"
                    font.pointSize: 28
                    font.bold: true
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
                text: "\u72B6\u6001\uFF1A\u5012\u8F66\u6A21\u5F0F"
                color: "#E91E63"
                font.pointSize: 11
            }

            Text {
                text: "\u5E27\u7387\uFF1A30fps"
                color: "#aaaaaa"
                font.pointSize: 11
            }

            Text {
                text: "\u5F55\u5236\uFF1A" + (isAutoRecording ? "\u5F55\u5236\u4E2D " + currentFrameCount + "/" + targetFrames + "\u5E27" : "\u7B49\u5F85\u4E2D")
                color: isAutoRecording ? "#ff3366" : "#aaaaaa"
                font.pointSize: 11
            }

            Text {
                text: "\u81EA\u52A8\u5F55\u5236\uFF1A" + (autoRecordSwitch.checked ? "\u6BCF" + intervalSpinBox.value + "\u79D2" : "\u5173\u95ED")
                color: autoRecordSwitch.checked ? "#E91E63" : "#aaaaaa"
                font.pointSize: 11
                Layout.alignment: Qt.AlignRight
            }
        }
    }

    // 时间水印更新定时器
    Timer {
        interval: 1000
        running: true
        repeat: true
    }
}
