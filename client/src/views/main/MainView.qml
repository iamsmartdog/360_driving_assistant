import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

//设置主窗口
Window {
    id: mainWindow
    width: 900
    height: 600
    title: "360驾驶助手"
    visible: true
    color: "white"

    //切换界面管理
    StackView {
        id: stackView
        anchors.fill: parent
        //开启动画
        initialItem: "SplashView.qml"

        //动画进入效果
        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        //动画退出效果
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }
    }
}
