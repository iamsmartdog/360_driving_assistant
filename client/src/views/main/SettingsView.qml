import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//主框架界面（左侧面板固定 + 右侧Loader动态切换）
Rectangle {
    id: settingsView

    // 整体渐变背景
    gradient: Gradient {
        GradientStop { position: 0.0; color: "#061552" }
        GradientStop { position: 0.5; color: "#0A2164" }
        GradientStop { position: 1.0; color: "#0A2164" }
    }

    // 记录当前选中的按钮索引（用于高亮显示）
    property int activeButtonIndex: 0  // 默认选中"主界面"（索引0）
    property bool showHomeView: true    // true=显示主界面仪表盘, false=显示系统设置

    // 跳转到登录界面（未登录时点击头像触发）
    function navigateToLogin() {
        try {
            var current = settingsView
            for (var i = 0; i < 15; i++) {
                current = current.parent
                if (current && current.push) {
                    current.push("LoginView.qml")
                    break
                }
            }
        } catch (e) {
            console.log("跳转登录失败:", e)
        }
    }

    // 消息对话框
    MessageDialog {
        id: messageDialog
        title: "提示"
        text: ""
        icon: StandardIcon.Information
        standardButtons: StandardButton.Ok
        property bool isConnectionSuccess: false
    }

    // WiFi 连接弹窗（点击顶部 WiFi 图标打开，扫描并连接手机热点等）
    Popup {
        id: wifiDialog
        x: (settingsView.width - width) / 2
        y: (settingsView.height - height) / 2
        width: 360
        height: 440
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#0C2972"; border.color: "#1B3A8A"; border.width: 1; radius: 12 }

        property string selectedSsid: ""

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 标题 + 刷新
            RowLayout {
                Layout.fillWidth: true
                Text { text: "连接WiFi"; color: "#DDE5F7"; font.pointSize: 16; font.bold: true; Layout.fillWidth: true }
                Button {
                    text: systemViewModel.wifiScanning ? "扫描中..." : "刷新"
                    enabled: !systemViewModel.wifiScanning
                    onClicked: systemViewModel.refreshWifiNetworks()
                }
            }

            // 网络列表
            ListView {
                id: wifiListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: systemViewModel.wifiNetworks
                spacing: 4
                delegate: Rectangle {
                    width: wifiListView.width
                    height: 38
                    radius: 6
                    color: wifiDialog.selectedSsid === modelData ? "#1E3F8F" : "transparent"
                    border.color: wifiDialog.selectedSsid === modelData ? "#3B90E4" : "transparent"
                    border.width: 1
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData; color: "#DDE5F7"; font.pointSize: 12
                        elide: Text.ElideRight; width: parent.width - 24
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: wifiDialog.selectedSsid = modelData }
                }
                Text {
                    anchors.centerIn: parent
                    visible: systemViewModel.wifiNetworks.length === 0
                    text: systemViewModel.wifiScanning ? "正在扫描..." : "无可用网络，点击刷新"
                    color: "#8CA0C4"; font.pointSize: 12
                }
            }

            // 密码输入
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: "密码"; color: "#8CA0C4"; font.pointSize: 12 }
                TextField {
                    id: wifiPwdInput
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: "输入WiFi密码"
                    color: "#DDE5F7"
                    background: Rectangle { color: "#000A3D"; border.color: "#1B3A8A"; radius: 6; implicitHeight: 34 }
                }
            }

            Text {
                Layout.fillWidth: true
                text: wifiDialog.selectedSsid === "" ? "请选择一个网络" : "已选择: " + wifiDialog.selectedSsid
                color: "#8CA0C4"; font.pointSize: 10
                elide: Text.ElideRight
            }

            // 操作按钮
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: wifiDialog.close() }
                Button {
                    text: "连接"
                    enabled: wifiDialog.selectedSsid !== ""
                    highlighted: true
                    onClicked: systemViewModel.connectWifi(wifiDialog.selectedSsid, wifiPwdInput.text)
                }
            }

            // 结果提示
            Text {
                id: wifiResultText
                Layout.fillWidth: true
                font.pointSize: 10
                horizontalAlignment: Text.AlignHCenter
                color: "#FB2E3A"
            }
        }

        onOpened: {
            wifiResultText.text = ""
            wifiDialog.selectedSsid = ""
            wifiPwdInput.text = ""
            systemViewModel.refreshWifiNetworks()
        }

        Connections {
            target: systemViewModel
            onWifiConnectResult: {
                wifiResultText.color = success ? "#4CAF50" : "#FB2E3A"
                wifiResultText.text = message
                if (success) closeTimer.start()
            }
        }
        Timer { id: closeTimer; interval: 1200; onTriggered: wifiDialog.close() }
    }

    // 确认对话框（用于恢复出厂设置、重启系统等危险操作）
    MessageDialog {
        id: confirmDialog
        title: "确认"
        text: ""
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Yes | StandardButton.No
        property var onConfirm: null
        onYes: {
            if (onConfirm) {
                onConfirm()
                onConfirm = null
            }
        }
    }

    // 主界面分为左右侧，从左到右创建
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==================== 左侧面板 ====================
        Rectangle {
            id: leftView
            Layout.preferredWidth: parent.width * 0.22
            Layout.minimumWidth: 200
            Layout.fillHeight: true
            color: "transparent"

            // 左侧半透明背景
            Rectangle {
                anchors.fill: parent
                color: "#000019"
                opacity: 0.95
                radius: 0
            }

            // 从上到下布局
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // ===== 顶部：系统Logo + 标题 =====
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 6

                    // Logo图标（圆形 + 文字）
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                        radius: 32
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#FB2E3A" }
                            GradientStop { position: 1.0; color: "#0A2164" }
                        }

                        // 呼吸光环
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 8
                            height: parent.height + 8
                            radius: parent.radius + 4
                            color: "transparent"
                            border.color: "#FB2E3A"
                            border.width: 1.5

                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.3; to: 0.9; duration: 2000 }
                                NumberAnimation { from: 0.9; to: 0.3; duration: 2000 }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "360"
                            color: "white"
                            font.pointSize: 18
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "智能行车辅助"
                        color: "#DDE5F7"
                        font.pointSize: 13
                        font.bold: true
                    }
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#1B3A8A"
                    Layout.topMargin: 4
                }

                // ===== 用户信息区 =====
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    spacing: 8

                    // 用户头像
                    Rectangle {
                        id: userAvatarRect
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        radius: 21
                        color: "#113180"
                        border.color: "#3B90E4"
                        border.width: 1.5

                        Text {
                            anchors.centerIn: parent
                            text: loginViewModel && loginViewModel.currentAccount ? "👤" : "🔑"
                            font.pointSize: 18
                        }

                        // 点击头像：未登录→跳转LoginView，已登录→弹出账号信息
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (loginViewModel && loginViewModel.currentAccount) {
                                    // 已登录，显示账号弹窗
                                    accountPopup.visible = !accountPopup.visible
                                } else {
                                    // 未登录，跳转登录界面
                                    navigateToLogin()
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: loginViewModel && loginViewModel.currentUserNickname ? loginViewModel.currentUserNickname : "未登录"
                            color: "#DDE5F7"
                            font.pointSize: 13
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: loginViewModel ? loginViewModel.currentAccount : ""
                            color: "#8CA0C4"
                            font.pointSize: 9
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#1B3A8A"
                }

                // ===== 功能按钮列表 =====
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    Repeater {
                        model: [
                            { text: "主界面", page: "home",                    color: "#2196F3", icon: "home",      special: false },
                            { text: "视频记录", page: "VideRecordView.qml",    color: "#4CAF50", icon: "video",     special: false },
                            { text: "行车模式", page: "DrivingModeView.qml",   color: "#4CAF50", icon: "car",       special: true  },
                            { text: "倒车模式", page: "ReverseModeView.qml", color: "#E91E63", icon: "reverse",   special: true  },
                            { text: "俯视模式", page: "BirdView.qml",          color: "#FE6F4C", icon: "birdview",  special: true  },
                            { text: "特征记录", page: "FeatureRecordView.qml", color: "#4CAF50", icon: "search",    special: false },
                            { text: "系统设置", page: "settings",              color: "#2196F3", icon: "settings",  special: false }
                        ]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            radius: 10
                            color: settingsView.activeButtonIndex === index ? "#1E3F8F" : "transparent"

                            // 选中时的左边高亮条
                            Rectangle {
                                visible: settingsView.activeButtonIndex === index
                                width: 4
                                height: parent.height * 0.6
                                radius: 2
                                color: "#FB2E3A"
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // 悬停效果
                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: "#ffffff"
                                opacity: buttonMouseArea.containsMouse && settingsView.activeButtonIndex !== index ? 0.08 : 0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 10

                                LineIcon {
                                    name: modelData.icon
                                    color: settingsView.activeButtonIndex === index ? "#ffffff" : "#A5B9EF"
                                    size: 22
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Text {
                                    text: modelData.text
                                    color: settingsView.activeButtonIndex === index ? "#ffffff" : "#A5B9EF"
                                    font.pointSize: 15
                                    font.bold: settingsView.activeButtonIndex === index
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                // 箭头指示器（选中时显示）
                                Text {
                                    text: "›"
                                    color: settingsView.activeButtonIndex === index ? "#ffffff" : "transparent"
                                    font.pointSize: 20
                                    font.bold: true
                                    Layout.alignment: Qt.AlignVCenter

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }
                            }

                            MouseArea {
                                id: buttonMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    settingsView.activeButtonIndex = index

                                    if (modelData.special) {
                                        // 特殊模式：弹出独立窗口
                                        rightLoader.source = ""
                                        var component = Qt.createComponent(modelData.page)
                                        if (component.status === Component.Ready) {
                                            var window = component.createObject(settingsView)
                                            window.settingsViewRef = settingsView
                                            window.show()
                                        } else {
                                            console.log("创建窗口失败:", component.errorString())
                                        }
                                    } else if (modelData.page === "home" || modelData.page === "settings") {
                                        // 主界面 / 系统设置：切换右侧面板内容
                                        rightLoader.source = ""
                                        settingsView.showHomeView = (modelData.page === "home")
                                    } else if (modelData.page) {
                                        // 有页面的按钮：切换右侧
                                        rightLoader.source = modelData.page
                                    }
                                }
                            }
                        }
                    }
                }

                // ===== 底部连接状态 =====
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 8
                    color: "#000019"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        // 连接状态指示灯（带呼吸动画）
                        Rectangle {
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                            radius: 5
                            color: networkViewModel && networkViewModel.isConnectedQml() ? "#4CAF50" : "#FB2E3A"

                            // 在线时呼吸动画
                            SequentialAnimation on opacity {
                                running: networkViewModel && networkViewModel.isConnectedQml()
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.5; to: 1.0; duration: 1500 }
                                NumberAnimation { from: 1.0; to: 0.5; duration: 1500 }
                            }
                        }

                        Text {
                            text: networkViewModel && networkViewModel.isConnectedQml() ? "已连接" : "未连接"
                            color: networkViewModel && networkViewModel.isConnectedQml() ? "#4CAF50" : "#FB2E3A"
                            font.pointSize: 11
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "V1.0"
                            color: "#5A6A8A"
                            font.pointSize: 10
                        }
                    }
                }
            }

            // 账号信息弹窗（悬浮在头像上方，不影响布局）
            Rectangle {
                id: accountPopup
                visible: false
                x: 10
                y: userAvatarRect.y - 100  // 在头像上方
                width: 190
                height: 90
                color: "#0C2972"
                border.color: "#3B90E4"
                border.width: 1
                radius: 8
                z: 200

                // 小三角箭头（指向下方头像）
                Canvas {
                    width: 14
                    height: 10
                    x: parent.width / 2 - 7
                    y: parent.height
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.fillStyle = "#0C2972"
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(7, 10)
                        ctx.lineTo(14, 0)
                        ctx.closePath()
                        ctx.fill()
                    }
                    Component.onCompleted: requestPaint()
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        text: "账号：" + (loginViewModel ? loginViewModel.currentAccount : "")
                        color: "#DDE5F7"; font.pointSize: 12
                    }
                    Text {
                        text: "昵称：" + (loginViewModel ? loginViewModel.currentUserNickname : "")
                        color: "#DDE5F7"; font.pointSize: 12
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: accountPopup.visible = false
                }
            }

            // 点击左侧面板其他区域关闭弹窗
            MouseArea {
                anchors.fill: parent
                z: 199
                onClicked: accountPopup.visible = false
                visible: accountPopup.visible
            }
        }

        // ==================== 右侧面板 ====================
        Rectangle {
            id: rightView
            Layout.preferredWidth: parent.width * 0.65
            Layout.minimumWidth: 420
            Layout.fillHeight: true
            color: "transparent"

            // 右侧半透明背景
            Rectangle {
                anchors.fill: parent
                color: "#0A2164"
                opacity: 0.3
            }

            // ===== 主界面仪表盘（showHomeView=true 时显示）=====
            Rectangle {
                id: defaultHomeContent
                anchors.fill: parent
                visible: !rightLoader.item && settingsView.showHomeView
                color: "transparent"

                // 时钟定时器
                Timer {
                    interval: 1000; running: true; repeat: true
                    onTriggered: {
                        var now = new Date()
                        clockText.text = Qt.formatDateTime(now, "hh:mm")
                        dateText.text = Qt.formatDateTime(now, "MM月dd日 ddd")
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 40
                    anchors.rightMargin: 40
                    anchors.topMargin: 20
                    anchors.bottomMargin: 20
                    spacing: 16

                    // ===== 顶部状态栏 =====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: 10
                        color: "#000A3D"
                        border.color: "#1B3A8A"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 16

                            // WiFi图标
                            Canvas {
                                width: 18; height: 14
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#3B90E4"; ctx.lineWidth = 2
                                    ctx.beginPath(); ctx.arc(9, 12, 7, Math.PI, 0); ctx.stroke()
                                    ctx.beginPath(); ctx.arc(9, 12, 4.5, Math.PI, 0); ctx.stroke()
                                    ctx.fillStyle = "#3B90E4"
                                    ctx.beginPath(); ctx.arc(9, 12, 1.5, 0, Math.PI*2); ctx.fill()
                                }
                                Component.onCompleted: requestPaint()
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: wifiDialog.open() }
                            }
                            Text {
                                text: "WiFi"; color: "#8CA0C4"; font.pointSize: 11
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: wifiDialog.open() }
                            }

                            // 蓝牙图标
                            Canvas {
                                width: 12; height: 18
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#3B90E4"; ctx.lineWidth = 1.8
                                    ctx.beginPath()
                                    ctx.moveTo(6, 2); ctx.lineTo(6, 16)
                                    ctx.moveTo(6, 2); ctx.lineTo(10, 6); ctx.lineTo(2, 12); ctx.lineTo(6, 16)
                                    ctx.moveTo(6, 2); ctx.lineTo(2, 6); ctx.lineTo(10, 12); ctx.lineTo(6, 16)
                                    ctx.stroke()
                                }
                                Component.onCompleted: requestPaint()
                            }
                            Text { text: "蓝牙"; color: "#8CA0C4"; font.pointSize: 11 }

                            Item { Layout.fillWidth: true }

                            // 日期
                            Text {
                                id: dateText
                                text: "08月05日 周二"
                                color: "#8CA0C4"; font.pointSize: 12
                            }

                            Rectangle { width: 1; height: 16; color: "#1B3A8A" }

                            // 时间
                            Text {
                                id: clockText
                                text: "14:30"
                                color: "#DDE5F7"; font.pointSize: 16; font.bold: true
                            }

                            Rectangle { width: 1; height: 16; color: "#1B3A8A" }

                            // 天气小图标 + 温度
                            Text { text: "☀"; font.pointSize: 14 }
                            Text { text: "28°"; color: "#DDE5F7"; font.pointSize: 13 }
                        }
                    }

                    // ===== 主内容区域 =====
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 16

                        // ----- 左：3D车模俯视图（带立体光影）-----
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 260
                            Layout.minimumWidth: 180
                            radius: 16
                            color: "#0C2972"
                            border.color: "#1B3A8A"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                // 标题行
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text {
                                        text: "🚗 全景俯视"
                                        color: "#8CA0C4"; font.pointSize: 12
                                        Layout.fillWidth: true
                                    }
                                    // 360标徽
                                    Rectangle {
                                        width: 32; height: 18; radius: 9
                                        color: "#153280"
                                        border.color: "#3B90E4"; border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "360°"; color: "#3B90E4"
                                            font.pointSize: 8; font.bold: true
                                        }
                                    }
                                }

                                // 3D车辆俯视图（带光影立体感）
                                Canvas {
                                    id: carCanvas
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    property real scanAngle: -Math.PI / 2
                                    // 尺寸变化时重绘，否则缩放窗口时画面会被拉伸变形
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        var w = width, h = height
                                        var cx = w / 2, cy = h / 2

                                        // ===== 1. 360°全景辅助圈（外圈雷达扫描）=====
                                        ctx.strokeStyle = "rgba(59,144,228,0.10)"
                                        ctx.lineWidth = 1
                                        for (var i = 1; i <= 4; i++) {
                                            ctx.beginPath()
                                            ctx.ellipse(cx, cy, w*0.10*i, h*0.09*i)
                                            ctx.stroke()
                                        }
                                        // 十字辅助线
                                        ctx.strokeStyle = "rgba(59,144,228,0.08)"
                                        ctx.setLineDash([4, 6])
                                        ctx.beginPath()
                                        ctx.moveTo(cx, cy - h*0.4); ctx.lineTo(cx, cy + h*0.4)
                                        ctx.moveTo(cx - w*0.4, cy); ctx.lineTo(cx + w*0.4, cy)
                                        ctx.stroke()
                                        ctx.setLineDash([])

                                        // 方向标记 N/E/S/W
                                        ctx.fillStyle = "rgba(140,160,196,0.5)"
                                        ctx.font = "9px sans-serif"
                                        ctx.textAlign = "center"
                                        ctx.fillText("前", cx, cy - h*0.42 + 10)
                                        ctx.fillText("后", cx, cy + h*0.42 - 2)
                                        ctx.fillText("左", cx - w*0.42 - 6, cy + 3)
                                        ctx.fillText("右", cx + w*0.42 + 6, cy + 3)

                                        // ===== 2. 车辆投影（柔和椭圆阴影）=====
                                        var shadowGrad = ctx.createRadialGradient(cx+4, cy+8, 2, cx+4, cy+8, w*0.28)
                                        shadowGrad.addColorStop(0, "rgba(0,0,0,0.5)")
                                        shadowGrad.addColorStop(1, "rgba(0,0,0,0)")
                                        ctx.fillStyle = shadowGrad
                                        ctx.beginPath()
                                        ctx.ellipse(cx+4, cy+8, w*0.26, h*0.20)
                                        ctx.fill()

                                        // ===== 3. 车身尺寸与圆角 =====
                                        var bw = w*0.26, bh = h*0.42
                                        var bx = cx-bw/2, by = cy-bh/2
                                        var r = 18

                                        // 车身底盘（深色，模拟车身厚度/侧面）
                                        ctx.fillStyle = "#0a1a4a"
                                        roundRect(ctx, bx-2, by+3, bw+4, bh+2, r+1)
                                        ctx.fill()

                                        // ===== 4. 车身主体（左上亮右下暗的渐变，模拟光照）=====
                                        var bodyGrad = ctx.createLinearGradient(bx, by, bx+bw, by+bh)
                                        bodyGrad.addColorStop(0, "#4A9AE4")
                                        bodyGrad.addColorStop(0.4, "#2E6FC0")
                                        bodyGrad.addColorStop(0.7, "#1B4A9A")
                                        bodyGrad.addColorStop(1, "#0F2E70")
                                        ctx.fillStyle = bodyGrad
                                        roundRect(ctx, bx, by, bw, bh, r)
                                        ctx.fill()

                                        // 车身边缘高光（左上）
                                        ctx.strokeStyle = "rgba(150,200,255,0.5)"
                                        ctx.lineWidth = 1.5
                                        ctx.beginPath()
                                        ctx.moveTo(bx+r, by+0.5)
                                        ctx.lineTo(bx+bw-r, by+0.5)
                                        ctx.quadraticCurveTo(bx+bw-0.5, by, bx+bw-0.5, by+r)
                                        ctx.stroke()

                                        // 车身边缘阴影（右下）
                                        ctx.strokeStyle = "rgba(0,0,0,0.4)"
                                        ctx.lineWidth = 1
                                        ctx.beginPath()
                                        ctx.moveTo(bx+r, by+bh-0.5)
                                        ctx.lineTo(bx+bw-r, by+bh-0.5)
                                        ctx.quadraticCurveTo(bx+bw-0.5, by+bh, bx+bw-0.5, by+bh-r)
                                        ctx.stroke()

                                        // ===== 5. 引擎盖（前部，略亮渐变）=====
                                        var hoodGrad = ctx.createLinearGradient(0, by, 0, by+bh*0.18)
                                        hoodGrad.addColorStop(0, "rgba(120,180,240,0.25)")
                                        hoodGrad.addColorStop(1, "rgba(120,180,240,0)")
                                        ctx.fillStyle = hoodGrad
                                        roundRect(ctx, bx+bw*0.1, by+bh*0.02, bw*0.8, bh*0.16, r*0.5)
                                        ctx.fill()

                                        // 引擎盖中线
                                        ctx.strokeStyle = "rgba(255,255,255,0.12)"
                                        ctx.lineWidth = 1
                                        ctx.beginPath()
                                        ctx.moveTo(cx, by+4)
                                        ctx.lineTo(cx, by+bh*0.12)
                                        ctx.stroke()

                                        // ===== 6. 前挡风玻璃（深色玻璃 + 反光高光）=====
                                        var glassGrad = ctx.createLinearGradient(0, by+bh*0.12, 0, by+bh*0.35)
                                        glassGrad.addColorStop(0, "rgba(10,20,50,0.85)")
                                        glassGrad.addColorStop(0.5, "rgba(30,60,120,0.7)")
                                        glassGrad.addColorStop(1, "rgba(80,140,220,0.5)")
                                        ctx.fillStyle = glassGrad
                                        ctx.beginPath()
                                        ctx.moveTo(bx+bw*0.16, by+bh*0.13)
                                        ctx.lineTo(bx+bw*0.84, by+bh*0.13)
                                        ctx.lineTo(bx+bw*0.72, by+bh*0.36)
                                        ctx.lineTo(bx+bw*0.28, by+bh*0.36)
                                        ctx.closePath()
                                        ctx.fill()

                                        // 前挡风玻璃反光（斜向高光）
                                        ctx.fillStyle = "rgba(200,230,255,0.18)"
                                        ctx.beginPath()
                                        ctx.moveTo(bx+bw*0.20, by+bh*0.15)
                                        ctx.lineTo(bx+bw*0.45, by+bh*0.15)
                                        ctx.lineTo(bx+bw*0.38, by+bh*0.34)
                                        ctx.lineTo(bx+bw*0.22, by+bh*0.34)
                                        ctx.closePath()
                                        ctx.fill()

                                        // ===== 7. 车顶（中间，带天窗）=====
                                        var roofGrad = ctx.createLinearGradient(bx, by+bh*0.35, bx, by+bh*0.65)
                                        roofGrad.addColorStop(0, "rgba(70,130,200,0.3)")
                                        roofGrad.addColorStop(1, "rgba(40,80,150,0.3)")
                                        ctx.fillStyle = roofGrad
                                        ctx.fillRect(bx+bw*0.10, by+bh*0.36, bw*0.80, bh*0.29)

                                        // 天窗（深色玻璃矩形）
                                        ctx.fillStyle = "rgba(10,20,50,0.7)"
                                        roundRect(ctx, bx+bw*0.22, by+bh*0.42, bw*0.56, bh*0.18, 6)
                                        ctx.fill()
                                        // 天窗反光
                                        ctx.fillStyle = "rgba(150,200,255,0.12)"
                                        roundRect(ctx, bx+bw*0.24, by+bh*0.43, bw*0.25, bh*0.16, 4)
                                        ctx.fill()

                                        // ===== 8. 后挡风玻璃 =====
                                        var rearGlassGrad = ctx.createLinearGradient(0, by+bh*0.65, 0, by+bh*0.88)
                                        rearGlassGrad.addColorStop(0, "rgba(80,140,220,0.5)")
                                        rearGlassGrad.addColorStop(0.5, "rgba(30,60,120,0.7)")
                                        rearGlassGrad.addColorStop(1, "rgba(10,20,50,0.85)")
                                        ctx.fillStyle = rearGlassGrad
                                        ctx.beginPath()
                                        ctx.moveTo(bx+bw*0.28, by+bh*0.65)
                                        ctx.lineTo(bx+bw*0.72, by+bh*0.65)
                                        ctx.lineTo(bx+bw*0.84, by+bh*0.87)
                                        ctx.lineTo(bx+bw*0.16, by+bh*0.87)
                                        ctx.closePath()
                                        ctx.fill()

                                        // ===== 9. 后备箱 =====
                                        var trunkGrad = ctx.createLinearGradient(0, by+bh*0.88, 0, by+bh)
                                        trunkGrad.addColorStop(0, "rgba(120,180,240,0)")
                                        trunkGrad.addColorStop(1, "rgba(120,180,240,0.2)")
                                        ctx.fillStyle = trunkGrad
                                        roundRect(ctx, bx+bw*0.1, by+bh*0.86, bw*0.8, bh*0.12, r*0.5)
                                        ctx.fill()

                                        // ===== 10. 车轮（四个，带轮毂立体感）=====
                                        var wheelW = bw*0.14, wheelH = bh*0.16
                                        var wheels = [
                                            [bx - wheelW*0.3, by + bh*0.16],
                                            [bx + bw - wheelW*0.7, by + bh*0.16],
                                            [bx - wheelW*0.3, by + bh*0.68],
                                            [bx + bw - wheelW*0.7, by + bh*0.68]
                                        ]
                                        for (var wi = 0; wi < wheels.length; wi++) {
                                            var wx = wheels[wi][0], wy = wheels[wi][1]
                                            // 轮胎外圈（黑色）
                                            ctx.fillStyle = "#0a0a0a"
                                            roundRect(ctx, wx, wy, wheelW, wheelH, 3)
                                            ctx.fill()
                                            // 轮毂（深灰）
                                            ctx.fillStyle = "#2a2a2a"
                                            roundRect(ctx, wx+2, wy+2, wheelW-4, wheelH-4, 2)
                                            ctx.fill()
                                            // 轮毂高光
                                            ctx.fillStyle = "rgba(120,144,176,0.4)"
                                            roundRect(ctx, wx+3, wy+3, wheelW-6, wheelH*0.4, 2)
                                            ctx.fill()
                                        }

                                        // ===== 11. 后视镜（两个小突起）=====
                                        ctx.fillStyle = "#1B4A9A"
                                        ctx.beginPath()
                                        ctx.ellipse(bx+bw*0.05, by+bh*0.30, bw*0.05, bh*0.04, 0, 0, Math.PI*2)
                                        ctx.fill()
                                        ctx.beginPath()
                                        ctx.ellipse(bx+bw*0.95, by+bh*0.30, bw*0.05, bh*0.04, 0, 0, Math.PI*2)
                                        ctx.fill()

                                        // ===== 12. 前方指示箭头（车头方向，红色发光）=====
                                        // 发光底
                                        ctx.fillStyle = "rgba(251,46,58,0.3)"
                                        ctx.beginPath()
                                        ctx.arc(cx, by-6, 10, 0, Math.PI*2)
                                        ctx.fill()
                                        // 箭头
                                        ctx.fillStyle = "#FB2E3A"
                                        ctx.beginPath()
                                        ctx.moveTo(cx, by-12)
                                        ctx.lineTo(cx-6, by-3)
                                        ctx.lineTo(cx+6, by-3)
                                        ctx.closePath()
                                        ctx.fill()

                                        // ===== 13. 雷达扫描线（旋转扇形，使用 scanAngle）=====
                                        var scanR = Math.min(w, h) * 0.42
                                        var scanGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, scanR)
                                        scanGrad.addColorStop(0, "rgba(59,144,228,0.35)")
                                        scanGrad.addColorStop(1, "rgba(59,144,228,0)")
                                        ctx.fillStyle = scanGrad
                                        ctx.beginPath()
                                        ctx.moveTo(cx, cy)
                                        ctx.arc(cx, cy, scanR, carCanvas.scanAngle - 0.35, carCanvas.scanAngle + 0.05)
                                        ctx.closePath()
                                        ctx.fill()

                                        // 扫描线主线
                                        ctx.strokeStyle = "rgba(120,180,240,0.6)"
                                        ctx.lineWidth = 1.5
                                        ctx.beginPath()
                                        ctx.moveTo(cx, cy)
                                        ctx.lineTo(cx + Math.cos(carCanvas.scanAngle) * scanR, cy + Math.sin(carCanvas.scanAngle) * scanR)
                                        ctx.stroke()
                                    }

                                    // 圆角矩形辅助函数
                                    function roundRect(ctx, x, y, w, h, r) {
                                        ctx.beginPath()
                                        ctx.moveTo(x+r, y)
                                        ctx.lineTo(x+w-r, y)
                                        ctx.quadraticCurveTo(x+w, y, x+w, y+r)
                                        ctx.lineTo(x+w, y+h-r)
                                        ctx.quadraticCurveTo(x+w, y+h, x+w-r, y+h)
                                        ctx.lineTo(x+r, y+h)
                                        ctx.quadraticCurveTo(x, y+h, x, y+h-r)
                                        ctx.lineTo(x, y+r)
                                        ctx.quadraticCurveTo(x, y, x+r, y)
                                        ctx.closePath()
                                    }
                                    Component.onCompleted: requestPaint()

                                    // 雷达扫描旋转动画
                                    SequentialAnimation {
                                        running: true; loops: Animation.Infinite
                                        NumberAnimation {
                                            target: carCanvas
                                            property: "scanAngle"
                                            from: -Math.PI / 2
                                            to: -Math.PI / 2 + Math.PI * 2
                                            duration: 4000
                                        }
                                    }
                                    onScanAngleChanged: requestPaint()
                                }

                                // 底部车辆状态
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text { text: "● P"; color: "#4CAF50"; font.pointSize: 11; font.bold: true }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "🌡 36.5°C"; color: "#8CA0C4"; font.pointSize: 10 }
                                    Text { text: "⚡ 12.3V"; color: "#8CA0C4"; font.pointSize: 10 }
                                }
                            }
                        }

                        // ----- 中：音乐快捷界面 -----
                        Rectangle {
                            id: musicCard
                            Layout.preferredWidth: 160
                            Layout.fillHeight: true
                            radius: 16
                            color: "#0C2972"
                            border.color: "#1B3A8A"
                            border.width: 1
                            property bool isPlaying: false

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Text {
                                    text: "🎵 音乐"
                                    color: "#8CA0C4"; font.pointSize: 12
                                    Layout.fillWidth: true
                                }

                                // 专辑封面
                                Rectangle {
                                    id: albumArt
                                    Layout.preferredWidth: 90
                                    Layout.preferredHeight: 90
                                    Layout.alignment: Qt.AlignHCenter
                                    radius: 12
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#FB2E3A" }
                                        GradientStop { position: 1.0; color: "#0A2164" }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "🎶"; font.pointSize: 32
                                    }

                                    RotationAnimation on rotation {
                                        running: musicCard.isPlaying
                                        loops: Animation.Infinite
                                        from: 0; to: 360
                                        duration: 8000
                                    }

                                    // 点击封面打开网易云音乐
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: systemViewModel.launchMusic() }
                                }

                                Text {
                                    text: "起风了"
                                    color: "#DDE5F7"; font.pointSize: 14; font.bold: true
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "买辣椒也用券"
                                    color: "#8CA0C4"; font.pointSize: 10
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                // 进度条
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 4
                                    radius: 2
                                    color: "#1B3A8A"

                                    Rectangle {
                                        width: parent.width * 0.35
                                        height: parent.height
                                        radius: 2
                                        color: "#3B90E4"
                                    }
                                }

                                // 播放控制
                                RowLayout {
                                    Layout.alignment: Qt.AlignHCenter
                                    spacing: 12

                                    Text {
                                        text: "⏮"; font.pointSize: 18; color: "#A5B9EF"
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                                    }

                                    Rectangle {
                                        width: 36; height: 36; radius: 18
                                        color: "#3B90E4"
                                        Layout.alignment: Qt.AlignVCenter

                                        Text {
                                            anchors.centerIn: parent
                                            text: musicCard.isPlaying ? "⏸" : "▶"
                                            color: "white"; font.pointSize: 14
                                        }

                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: musicCard.isPlaying = !musicCard.isPlaying
                                        }
                                    }

                                    Text {
                                        text: "⏭"; font.pointSize: 18; color: "#A5B9EF"
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                                    }
                                }
                            }
                        }

                        // ----- 右：天气 + 导航（各自独立卡片）-----
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 160
                            spacing: 16

                            // 天气控件（上）
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 110
                                radius: 16
                                color: "#0C2972"
                                border.color: "#1B3A8A"
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        ColumnLayout {
                                            spacing: 2
                                            Text { text: "28°"; color: "#ffffff"; font.pointSize: 42; font.bold: true }
                                            Text { text: "晴"; color: "#A5B9EF"; font.pointSize: 14 }
                                            Text { text: "深圳"; color: "#8CA0C4"; font.pointSize: 11 }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Canvas {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 50
                                            onPaint: {
                                                var ctx = getContext("2d")
                                                ctx.reset()
                                                var cx = 25, cy = 25
                                                ctx.strokeStyle = "#FFD700"; ctx.lineWidth = 2
                                                for (var i = 0; i < 8; i++) {
                                                    var a = i * Math.PI / 4
                                                    ctx.beginPath()
                                                    ctx.moveTo(cx + Math.cos(a)*14, cy + Math.sin(a)*14)
                                                    ctx.lineTo(cx + Math.cos(a)*20, cy + Math.sin(a)*20)
                                                    ctx.stroke()
                                                }
                                                ctx.fillStyle = "#FFD700"
                                                ctx.beginPath(); ctx.arc(cx, cy, 12, 0, Math.PI*2); ctx.fill()
                                            }
                                            Component.onCompleted: requestPaint()
                                        }
                                    }

                                    Item { Layout.fillHeight: true }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text { text: "↑32°"; color: "#FB2E3A"; font.pointSize: 11 }
                                        Text { text: "↓25°"; color: "#3B90E4"; font.pointSize: 11 }
                                        Item { Layout.fillWidth: true }
                                        Text { text: "湿度 65%"; color: "#8CA0C4"; font.pointSize: 10 }
                                    }
                                }
                            }

                            // 导航控件（下）
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 110
                                radius: 16
                                color: "#0C2972"
                                border.color: "#1B3A8A"
                                border.width: 1
                                clip: true

                                Canvas {
                                    id: mapCanvas
                                    anchors.fill: parent
                                    anchors.margins: 12   // 内缩，露出卡片底色与边框，让导航框清晰可见
                                    // 尺寸变化时重绘，否则缩放窗口时路线/坐标会错位
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        var w = width, h = height

                                        // 背景
                                        ctx.fillStyle = "#000A3D"
                                        ctx.fillRect(0, 0, w, h)

                                        // 网格
                                        ctx.strokeStyle = "rgba(59,144,228,0.08)"
                                        ctx.lineWidth = 1
                                        for (var x = 0; x <= w; x += 20) {
                                            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke()
                                        }
                                        for (var y = 0; y <= h; y += 20) {
                                            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke()
                                        }

                                        // 路线
                                        ctx.strokeStyle = "#3B90E4"; ctx.lineWidth = 3
                                        ctx.beginPath()
                                        ctx.moveTo(w*0.15, h*0.85)
                                        ctx.lineTo(w*0.15, h*0.5)
                                        ctx.quadraticCurveTo(w*0.15, h*0.25, w*0.4, h*0.25)
                                        ctx.lineTo(w*0.65, h*0.25)
                                        ctx.quadraticCurveTo(w*0.9, h*0.25, w*0.9, h*0.5)
                                        ctx.stroke()

                                        // 当前位置（红点）
                                        ctx.fillStyle = "#FB2E3A"
                                        ctx.beginPath(); ctx.arc(w*0.15, h*0.85, 6, 0, Math.PI*2); ctx.fill()
                                        ctx.strokeStyle = "white"; ctx.lineWidth = 2; ctx.stroke()

                                        // 目的地（蓝点）
                                        ctx.fillStyle = "#3B90E4"
                                        ctx.beginPath(); ctx.arc(w*0.9, h*0.5, 5, 0, Math.PI*2); ctx.fill()
                                    }
                                    Component.onCompleted: requestPaint()
                                }

                                // 导航信息覆盖层
                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 12
                                    spacing: 4

                                    Text {
                                        text: "→ 前方200米右转"
                                        color: "#DDE5F7"; font.pointSize: 13; font.bold: true
                                    }
                                    RowLayout {
                                        spacing: 8
                                        Text { text: "科技园南路"; color: "#8CA0C4"; font.pointSize: 11 }
                                        Item { Layout.preferredWidth: 8 }
                                        Text { text: "3.2km"; color: "#3B90E4"; font.pointSize: 11 }
                                        Text { text: "|"; color: "#1B3A8A"; font.pointSize: 11 }
                                        Text { text: "15:20到达"; color: "#8CA0C4"; font.pointSize: 11 }
                                    }
                                }

                                // 点击导航卡片打开高德导航
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: systemViewModel.launchNavigation() }
                            }
                        }
                    }
                }
            }

            // 默认的系统设置内容（Loader为空且非主界面时显示）
            // 内容较多，使用Flickable支持滚动
            Flickable {
                id: defaultSettingsContent
                anchors.fill: parent
                visible: !rightLoader.item && !settingsView.showHomeView
                anchors.leftMargin: 60
                anchors.rightMargin: 40
                anchors.topMargin: 40
                anchors.bottomMargin: 40
                contentWidth: width
                contentHeight: settingsColumn.implicitHeight + 20
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: "#3B90E4"
                        opacity: 0.5
                    }
                }

                ColumnLayout {
                    id: settingsColumn
                    width: parent.width
                    spacing: 20

                    // ===== 标题区域 =====
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "360度智能行车辅助系统"
                            color: "#ffffff"
                            font.pointSize: 24
                            font.bold: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 3
                            radius: 1.5
                            color: "#FB2E3A"
                        }

                        Text {
                            text: "系统设置"
                            color: "#8CA0C4"
                            font.pointSize: 14
                        }
                    }

                    // ===== 语言与输入法 =====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        radius: 12
                        color: "#0C2972"
                        border.color: "#1B3A8A"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            // 分区标题
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text { text: "🌐"; font.pointSize: 20 }
                                Text {
                                    text: "语言与输入法"
                                    color: "#DDE5F7"; font.pointSize: 15; font.bold: true
                                    Layout.fillWidth: true
                                }
                            }

                            // 系统语言
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "系统语言"
                                    color: "#8CA0C4"; font.pointSize: 13
                                    Layout.fillWidth: true
                                }

                                ComboBox {
                                    id: langCombo
                                    model: ["简体中文", "English"]
                                    font.pointSize: 13
                                    implicitWidth: 160
                                    implicitHeight: 34

                                    background: Rectangle {
                                        color: "#000A3D"
                                        border.color: langCombo.pressed ? "#3B90E4" : "#1B3A8A"
                                        border.width: langCombo.pressed ? 2 : 1
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        text: langCombo.displayText
                                        color: "#DDE5F7"; font: langCombo.font
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    delegate: ItemDelegate {
                                        width: langCombo.width
                                        contentItem: Text {
                                            text: modelData
                                            color: langCombo.currentIndex === index ? "#3B90E4" : "#DDE5F7"
                                            font.pointSize: 13; leftPadding: 10
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: langCombo.highlightedIndex === index ? "#153280" : "#000A3D"
                                        }
                                    }
                                    popup: Popup {
                                        y: langCombo.height + 2
                                        width: langCombo.width
                                        implicitHeight: Math.min(contentItem.implicitHeight, 200)
                                        padding: 0
                                        background: Rectangle {
                                            color: "#000A3D"; border.color: "#1B3A8A"
                                            border.width: 1; radius: 6
                                        }
                                        contentItem: ListView {
                                            clip: true; implicitHeight: contentHeight
                                            model: langCombo.popup.visible ? langCombo.popup.contentModel : null
                                            currentIndex: langCombo.highlightedIndex
                                        }
                                    }
                                }
                            }

                            // 键盘输入法
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "键盘输入法"
                                    color: "#8CA0C4"; font.pointSize: 13
                                    Layout.fillWidth: true
                                }

                                ComboBox {
                                    id: imeCombo
                                    model: ["默认输入法", "拼音输入", "手写输入"]
                                    font.pointSize: 13
                                    implicitWidth: 160
                                    implicitHeight: 34

                                    background: Rectangle {
                                        color: "#000A3D"
                                        border.color: imeCombo.pressed ? "#3B90E4" : "#1B3A8A"
                                        border.width: imeCombo.pressed ? 2 : 1
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        text: imeCombo.displayText
                                        color: "#DDE5F7"; font: imeCombo.font
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    delegate: ItemDelegate {
                                        width: imeCombo.width
                                        contentItem: Text {
                                            text: modelData
                                            color: imeCombo.currentIndex === index ? "#3B90E4" : "#DDE5F7"
                                            font.pointSize: 13; leftPadding: 10
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: imeCombo.highlightedIndex === index ? "#153280" : "#000A3D"
                                        }
                                    }
                                    popup: Popup {
                                        y: imeCombo.height + 2
                                        width: imeCombo.width
                                        implicitHeight: Math.min(contentItem.implicitHeight, 200)
                                        padding: 0
                                        background: Rectangle {
                                            color: "#000A3D"; border.color: "#1B3A8A"
                                            border.width: 1; radius: 6
                                        }
                                        contentItem: ListView {
                                            clip: true; implicitHeight: contentHeight
                                            model: imeCombo.popup.visible ? imeCombo.popup.contentModel : null
                                            currentIndex: imeCombo.highlightedIndex
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ===== 日期和时间 =====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        radius: 12
                        color: "#0C2972"
                        border.color: "#1B3A8A"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text { text: "📅"; font.pointSize: 20 }
                                Text {
                                    text: "日期和时间"
                                    color: "#DDE5F7"; font.pointSize: 15; font.bold: true
                                    Layout.fillWidth: true
                                }
                            }

                            // 自动网络对时
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "自动网络对时"
                                    color: "#8CA0C4"; font.pointSize: 13
                                    Layout.fillWidth: true
                                }

                                // 自定义开关
                                Rectangle {
                                    id: autoTimeToggle
                                    property bool checked: true
                                    width: 46; height: 26; radius: 13
                                    color: checked ? "#3B90E4" : "#1B3A8A"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Rectangle {
                                        width: 20; height: 20; radius: 10; color: "#ffffff"
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: autoTimeToggle.checked ? parent.width - width - 3 : 3
                                        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                                    }
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: autoTimeToggle.checked = !autoTimeToggle.checked
                                    }
                                }
                            }

                            // 时区
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "时区"
                                    color: "#8CA0C4"; font.pointSize: 13
                                    Layout.fillWidth: true
                                }

                                ComboBox {
                                    id: timezoneCombo
                                    model: ["GMT+8 北京时间", "GMT+9 东京时间", "GMT+0 伦敦时间", "GMT-5 纽约时间"]
                                    font.pointSize: 13
                                    implicitWidth: 180
                                    implicitHeight: 34

                                    background: Rectangle {
                                        color: "#000A3D"
                                        border.color: timezoneCombo.pressed ? "#3B90E4" : "#1B3A8A"
                                        border.width: timezoneCombo.pressed ? 2 : 1
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        text: timezoneCombo.displayText
                                        color: "#DDE5F7"; font: timezoneCombo.font
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    delegate: ItemDelegate {
                                        width: timezoneCombo.width
                                        contentItem: Text {
                                            text: modelData
                                            color: timezoneCombo.currentIndex === index ? "#3B90E4" : "#DDE5F7"
                                            font.pointSize: 13; leftPadding: 10
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: timezoneCombo.highlightedIndex === index ? "#153280" : "#000A3D"
                                        }
                                    }
                                    popup: Popup {
                                        y: timezoneCombo.height + 2
                                        width: timezoneCombo.width
                                        implicitHeight: Math.min(contentItem.implicitHeight, 200)
                                        padding: 0
                                        background: Rectangle {
                                            color: "#000A3D"; border.color: "#1B3A8A"
                                            border.width: 1; radius: 6
                                        }
                                        contentItem: ListView {
                                            clip: true; implicitHeight: contentHeight
                                            model: timezoneCombo.popup.visible ? timezoneCombo.popup.contentModel : null
                                            currentIndex: timezoneCombo.highlightedIndex
                                        }
                                    }
                                }
                            }

                            // 24小时制
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "24小时制"
                                    color: "#8CA0C4"; font.pointSize: 13
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    id: hourFormatToggle
                                    property bool checked: true
                                    width: 46; height: 26; radius: 13
                                    color: checked ? "#3B90E4" : "#1B3A8A"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Rectangle {
                                        width: 20; height: 20; radius: 10; color: "#ffffff"
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: hourFormatToggle.checked ? parent.width - width - 3 : 3
                                        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                                    }
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: hourFormatToggle.checked = !hourFormatToggle.checked
                                    }
                                }
                            }
                        }
                    }

                    // ===== 本地存储设置（保留原有功能）=====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        radius: 12
                        color: "#0C2972"
                        border.color: "#1B3A8A"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 16

                            Text {
                                text: "💾"
                                font.pointSize: 28
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: "本地存储设置"
                                    color: "#DDE5F7"
                                    font.pointSize: 15
                                    font.bold: true
                                }

                                RowLayout {
                                    spacing: 10

                                    Text {
                                        text: "存储上限："
                                        color: "#8CA0C4"
                                        font.pointSize: 13
                                    }

                                    TextField {
                                        id: storageField
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 34
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pointSize: 14
                                        color: "#DDE5F7"
                                        leftPadding: 6; rightPadding: 6; topPadding: 4; bottomPadding: 4
                                        validator: IntValidator { bottom: 1; top: 1000 }

                                        background: Rectangle {
                                            color: "#000A3D"
                                            border.color: storageField.activeFocus ? "#3B90E4" : "#1B3A8A"
                                            border.width: storageField.activeFocus ? 2 : 1
                                            radius: 6
                                        }
                                    }

                                    Text {
                                        text: "GB"
                                        color: "#8CA0C4"
                                        font.pointSize: 13
                                    }

                                    Item { Layout.fillWidth: true }

                                    CheckBox {
                                        id: autoDeleteCheckbox
                                        checked: false

                                        indicator: Rectangle {
                                            implicitWidth: 20
                                            implicitHeight: 20
                                            radius: 4
                                            border.color: autoDeleteCheckbox.checked ? "#3B90E4" : "#1B3A8A"
                                            border.width: 1
                                            color: autoDeleteCheckbox.checked ? "#153280" : "#000A3D"

                                            Text {
                                                anchors.centerIn: parent
                                                text: autoDeleteCheckbox.checked ? "✓" : ""
                                                color: "#3B90E4"
                                                font.pointSize: 12
                                                font.bold: true
                                            }
                                        }

                                        contentItem: Text {
                                            text: "自动删除旧视频"
                                            color: "#A5B9EF"
                                            font.pointSize: 13
                                            leftPadding: autoDeleteCheckbox.indicator.width + 6
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ===== 关于本机 =====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: 12
                        color: "#0C2972"
                        border.color: "#1B3A8A"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text { text: "ℹ️"; font.pointSize: 20 }
                                Text {
                                    text: "关于本机"
                                    color: "#DDE5F7"; font.pointSize: 15; font.bold: true
                                    Layout.fillWidth: true
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 1
                                color: "#1B3A8A"
                            }

                            // 信息行组件（标签 + 值）
                            Repeater {
                                model: [
                                    { label: "车机型号", value: "360-Dashboard-Pro" },
                                    { label: "固件版本", value: "V1.0.0 Build 2026" },
                                    { label: "内核版本", value: "Linux 5.4.0-aarch64" },
                                    { label: "MCU 版本", value: "MCU-V2.3.1" },
                                    { label: "序列号",   value: "SN202608050001" }
                                ]

                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Text {
                                        text: modelData.label
                                        color: "#8CA0C4"; font.pointSize: 13
                                        Layout.preferredWidth: 100
                                    }
                                    Text {
                                        text: modelData.value
                                        color: "#DDE5F7"; font.pointSize: 13; font.bold: true
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }

                    // ===== 软件更新 =====
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        radius: 12
                        color: "#0C2972"
                        border.color: "#1B3A8A"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text { text: "🔄"; font.pointSize: 20 }
                                Text {
                                    text: "软件更新"
                                    color: "#DDE5F7"; font.pointSize: 15; font.bold: true
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16

                                // OTA 在线升级
                                Button {
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 40
                                    text: "OTA 在线升级"
                                    font.pointSize: 13; font.bold: true

                                    background: Rectangle {
                                        radius: 20
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: "#3B90E4" }
                                            GradientStop { position: 1.0; color: "#176DF7" }
                                        }
                                        Rectangle {
                                            anchors.fill: parent; radius: 20; color: "#ffffff"
                                            opacity: otaBtnArea.containsMouse ? 0.1 : 0
                                            Behavior on opacity { NumberAnimation { duration: 150 } }
                                        }
                                    }
                                    contentItem: Text {
                                        text: parent.text; font: parent.font; color: "#ffffff"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    MouseArea {
                                        id: otaBtnArea
                                        anchors.fill: parent; hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            messageDialog.icon = StandardIcon.Information
                                            messageDialog.title = "OTA 升级"
                                            messageDialog.text = "正在检查更新，请稍候...\n当前版本：V1.0.0 Build 2026"
                                            messageDialog.open()
                                        }
                                    }
                                }

                                // 本地 U 盘升级
                                Button {
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 40
                                    text: "本地 U 盘升级"
                                    font.pointSize: 13; font.bold: true

                                    background: Rectangle {
                                        radius: 20
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: "#2a6a3a" }
                                            GradientStop { position: 1.0; color: "#1a5a2a" }
                                        }
                                        Rectangle {
                                            anchors.fill: parent; radius: 20; color: "#ffffff"
                                            opacity: usbBtnArea.containsMouse ? 0.1 : 0
                                            Behavior on opacity { NumberAnimation { duration: 150 } }
                                        }
                                    }
                                    contentItem: Text {
                                        text: parent.text; font: parent.font; color: "#ffffff"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    MouseArea {
                                        id: usbBtnArea
                                        anchors.fill: parent; hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            messageDialog.icon = StandardIcon.Information
                                            messageDialog.title = "本地升级"
                                            messageDialog.text = "请将升级包放入 U 盘根目录后插入设备。\n支持的格式：update.zip"
                                            messageDialog.open()
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    // ===== 恢复出厂设置 + 重启系统 =====
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Item { Layout.fillWidth: true }

                        // 恢复出厂设置（危险操作，红色）
                        Button {
                            Layout.preferredWidth: 160
                            Layout.preferredHeight: 44
                            text: "恢复出厂设置"
                            font.pointSize: 14; font.bold: true

                            background: Rectangle {
                                radius: 22
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#FB2E3A" }
                                    GradientStop { position: 1.0; color: "#DC1D38" }
                                }
                                Rectangle {
                                    anchors.fill: parent; radius: 22; color: "#ffffff"
                                    opacity: resetBtnArea.containsMouse ? 0.15 : 0
                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                }
                            }
                            contentItem: Text {
                                text: parent.text; font: parent.font; color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            MouseArea {
                                id: resetBtnArea
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    confirmDialog.title = "恢复出厂设置"
                                    confirmDialog.text = "此操作将清除所有用户数据和设置，恢复到出厂状态。\n确定要继续吗？"
                                    confirmDialog.onConfirm = function() {
                                        messageDialog.icon = StandardIcon.Information
                                        messageDialog.title = "恢复出厂设置"
                                        messageDialog.text = "系统正在恢复出厂设置，请勿断电...\n恢复完成后将自动重启。"
                                        messageDialog.open()
                                    }
                                    confirmDialog.open()
                                }
                            }
                        }

                        // 重启系统
                        Button {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 44
                            text: "重启系统"
                            font.pointSize: 14; font.bold: true

                            background: Rectangle {
                                radius: 22
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#FE6F4C" }
                                    GradientStop { position: 1.0; color: "#FF8955" }
                                }
                                Rectangle {
                                    anchors.fill: parent; radius: 22; color: "#ffffff"
                                    opacity: rebootBtnArea.containsMouse ? 0.15 : 0
                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                }
                            }
                            contentItem: Text {
                                text: parent.text; font: parent.font; color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            MouseArea {
                                id: rebootBtnArea
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    confirmDialog.title = "重启系统"
                                    confirmDialog.text = "系统即将重启，未保存的数据将丢失。\n确定要重启吗？"
                                    confirmDialog.onConfirm = function() {
                                        messageDialog.icon = StandardIcon.Information
                                        messageDialog.title = "重启系统"
                                        messageDialog.text = "系统正在重启，请稍候..."
                                        messageDialog.open()
                                    }
                                    confirmDialog.open()
                                }
                            }
                        }
                    }

                    // ===== 保存设置按钮 =====
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Item { Layout.fillWidth: true }

                        Button {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 44
                            text: "保存设置"
                            font.pointSize: 15
                            font.bold: true

                            background: Rectangle {
                                radius: 22
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#3B90E4" }
                                    GradientStop { position: 1.0; color: "#176DF7" }
                                }

                                // 悬停高亮
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 22
                                    color: "#ffffff"
                                    opacity: saveButtonMouseArea.containsMouse ? 0.1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 150 } }
                                }
                            }

                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                id: saveButtonMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var storage = parseInt(storageField.text) || 10
                                    if (settingsViewModel) {
                                        settingsViewModel.storageSize = storage
                                        settingsViewModel.autoDelete = autoDeleteCheckbox.checked
                                        if (settingsViewModel.saveSettings()) {
                                            messageDialog.icon = StandardIcon.Information
                                            messageDialog.title = "提示"
                                            messageDialog.text = "设置已保存！"
                                            messageDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 组件加载完成时，自动加载已保存的配置
            Component.onCompleted: {
                if (settingsViewModel && settingsViewModel.hasSettings()) {
                    settingsViewModel.loadSettings()
                    storageField.text = settingsViewModel.storageSize.toString()
                    autoDeleteCheckbox.checked = settingsViewModel.autoDelete
                }
            }

            // Loader用于加载其他右侧面板
            Loader {
                id: rightLoader
                anchors.fill: parent
            }
        }
    }
}
