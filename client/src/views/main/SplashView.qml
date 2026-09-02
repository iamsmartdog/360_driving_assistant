import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//启动界面（FDBus自动发现）
Rectangle {
    id: splashView
    color: "white"

    // 显示文本
    Text {
        id: titleText
        anchors.centerIn: parent
        text: "360度智能行车辅助系统"
        color: "black"
        font.pointSize: 24

        opacity: 0

        //淡入动画效果
        NumberAnimation {
            target: titleText
            property: "opacity"
            from: 0
            to: 1
            duration: 2000
            running: true
        }
    }

    // 连接状态提示文本
    Text {
        id: statusText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: progressBar.top
        anchors.bottomMargin: 20
        text: "正在初始化..."
        color: "#666666"
        font.pointSize: 14
    }

    //加载进度条
    ProgressBar {
        id: progressBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
        width: 300

        value: 0

        //进度条动画
        NumberAnimation {
            target: progressBar
            property: "value"
            from: 0
            to: 1
            duration: 3000
            running: true
        }
    }

    // 未连接网络提示对话框
    MessageDialog {
        id: connectionFailedDialog
        title: "提示"
        text: "未连接到网络"
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok
        onAccepted: {
            Qt.quit()
        }
    }

    // 监听FDBus连接结果
    Connections {
        target: networkViewModel
        onConnectionSuccess: {
            statusText.text = "连接成功！"
            navigateTimer.viewName = "SettingsView.qml"
            navigateTimer.start()
        }
        onConnectionFailed: {
            connectionFailedDialog.open()
        }
    }

    // 延迟跳转定时器
    Timer {
        id: navigateTimer
        property string viewName: ""
        interval: 300
        onTriggered: {
            if (splashView.parent && splashView.parent.push) {
                splashView.parent.push(viewName)
            }
        }
    }

    // 进度条完成后自动通过FDBus发现服务端
    Timer {
        id: splashTimer
        interval: 3500
        running: true
        onTriggered: {
            if (networkViewModel && networkViewModel.isConnectedQml()) {
                // 已经连接，直接进主界面
                statusText.text = "连接成功！"
                navigateTimer.viewName = "SettingsView.qml"
                navigateTimer.start()
            } else {
                // 通过FDBus自动发现服务端
                statusText.text = "正在搜索服务器..."
                networkViewModel.connectToServer("driving_assistant")
            }
        }
    }
}
