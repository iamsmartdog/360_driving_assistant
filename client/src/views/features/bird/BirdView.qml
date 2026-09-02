import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

// 鸟瞰模式窗口 — 左右分屏: 左侧 AVM 鸟瞰拼接图, 右侧前置摄像头实时画面
// 启动即自动采集 + 录制 (每300帧自动分段保存 AVI)
Window {
    id: birdViewWindow
    width: 1200
    height: 800
    title: "俯视模式 - 360度智能行车辅助系统"
    color: "black"

    property var settingsViewRef: null
    property var viewModel: birdRecordViewModel

    onClosing: {
        if (viewModel) viewModel.stop()
        if (settingsViewRef) settingsViewRef.activeButtonIndex = 5
    }

    Component.onCompleted: {
        if (viewModel) viewModel.start()
    }

    // ========== 左侧: AVM 鸟瞰拼接图 ==========
    Rectangle {
        id: leftPanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width / 2
        color: "#1a1a1a"

        Image {
            id: avmImage
            anchors.fill: parent
            anchors.margins: 2
            source: (viewModel && viewModel.frameCounter > 0)
                    ? "image://videoframe/bird_avm?" + viewModel.frameCounter : ""
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false
        }

        // 左侧标签
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 8
            anchors.leftMargin: 8
            width: labelAvm.implicitWidth + 16
            height: labelAvm.implicitHeight + 8
            color: "#80000000"
            radius: 4

            Text {
                id: labelAvm
                anchors.centerIn: parent
                text: "鸟瞰图"
                color: "white"
                font.pointSize: 11
                font.bold: true
            }
        }

        // 加载提示
        Text {
            anchors.centerIn: parent
            text: {
                if (!viewModel) return ""
                if (viewModel.isRunning && viewModel.frameCounter === 0)
                    return "鸟瞰图构建中..."
                if (viewModel.frameCounter === 0)
                    return "点击启动"
                return ""
            }
            color: "#888888"
            font.pointSize: 16
            visible: text.length > 0
        }
    }

    // ========== 右侧: 前置摄像头实时画面 ==========
    Rectangle {
        id: rightPanel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width / 2
        color: "#1a1a1a"

        Image {
            id: camImage
            anchors.fill: parent
            anchors.margins: 2
            source: (viewModel && viewModel.frameCounter > 0)
                    ? "image://videoframe/bird_cam?" + viewModel.frameCounter : ""
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false
        }

        // 右侧标签
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 8
            anchors.leftMargin: 8
            width: labelCam.implicitWidth + 16
            height: labelCam.implicitHeight + 8
            color: "#80000000"
            radius: 4

            Text {
                id: labelCam
                anchors.centerIn: parent
                text: "实时摄像头"
                color: "white"
                font.pointSize: 11
                font.bold: true
            }
        }

        // 摄像头未打开提示
        Text {
            anchors.centerIn: parent
            text: "摄像头画面等待中..."
            color: "#888888"
            font.pointSize: 14
            visible: viewModel ? viewModel.frameCounter === 0 : true
        }
    }

    // ========== 录制状态指示器 (右上角) ==========
    Rectangle {
        id: recordIndicator
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 8
        width: recordRow.implicitWidth + 20
        height: recordRow.implicitHeight + 10
        color: "#80000000"
        radius: 4
        visible: viewModel ? viewModel.isRecording : false

        Row {
            id: recordRow
            anchors.centerIn: parent
            spacing: 6

            // 红色录制圆点 (闪烁动画)
            Rectangle {
                id: recordDot
                width: 10
                height: 10
                radius: 5
                color: "#ff3333"
                anchors.verticalCenter: parent.verticalCenter

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: recordIndicator.visible
                    NumberAnimation { from: 1.0; to: 0.3; duration: 500 }
                    NumberAnimation { from: 0.3; to: 1.0; duration: 500 }
                }
            }

            Text {
                text: {
                    if (!viewModel) return ""
                    var mm = Math.floor(viewModel.recordDuration / 60)
                    var ss = viewModel.recordDuration % 60
                    var timeStr = (mm < 10 ? "0" : "") + mm + ":" + (ss < 10 ? "0" : "") + ss
                    return "录制中 " + timeStr + " (" + viewModel.writtenFrames + "/300帧)"
                }
                color: "white"
                font.pointSize: 10
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ========== 关闭按钮 (底部居中) ==========
    Button {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 12
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
        onClicked: birdViewWindow.close()
    }
}
