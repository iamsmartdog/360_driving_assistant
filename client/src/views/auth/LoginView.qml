import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//登录界面（使用Rectangle，适配StackView）
Rectangle {
    id: loginView
    color: "#f5f5f5"

    // 消息对话框
    MessageDialog {
        id: messageDialog
        title: "提示"
        text: ""
        icon: StandardIcon.Information
        standardButtons: StandardButton.Ok
        property bool isLoginSuccess: false
        onAccepted: {
            if (isLoginSuccess) {
                try {
                    var current = loginView
                    for (var i = 0; i < 15; i++) {
                        current = current.parent
                        if (current && current.push) {
                            // 登录成功后进入主界面，替换掉登录页
                            current.replace("SettingsView.qml")
                            break
                        }
                    }
                } catch (e) {
                    console.log("跳转失败:", e)
                }
            }
        }
    }

    // 监听登录结果
    Connections {
        target: loginViewModel
        onLoginSuccess: {
            messageDialog.icon = StandardIcon.Information
            messageDialog.title = "登录成功"
            messageDialog.text = "欢迎 " + nickname + "！即将进入系统主界面。"
            messageDialog.isLoginSuccess = true
            messageDialog.open()
        }
        onLoginFailed: {
            messageDialog.icon = StandardIcon.Critical
            messageDialog.title = "登录失败"
            messageDialog.text = errorMessage
            messageDialog.isLoginSuccess = false
            messageDialog.open()
        }
        onValidationError: {
            messageDialog.icon = StandardIcon.Warning
            messageDialog.title = "输入错误"
            messageDialog.text = errorMessage
            messageDialog.isLoginSuccess = false
            messageDialog.open()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        // 标题
        Text {
            id: titleText
            text: "360度智能行车辅助系统"
            color: "#333333"
            font.pointSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // 账号输入框
        TextField {
            id: accountField
            placeholderText: "请输入账号"
            maximumLength: 11
            Layout.preferredWidth: 350
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            font.pointSize: 14
            color: "#333333"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8

            validator: RegExpValidator { regExp: /[a-zA-Z0-9_]{0,11}/ }

            background: Rectangle {
                color: "white"
                border.color: accountField.activeFocus ? "#2196F3" : "#cccccc"
                border.width: accountField.activeFocus ? 2 : 1
                radius: 4
            }
        }

        // 密码输入框
        TextField {
            id: passwordField
            placeholderText: "请输入 10 位英文字母、数字、下划线"
            maximumLength: 10
            echoMode: TextInput.Password
            Layout.preferredWidth: 350
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            font.pointSize: 14
            color: "#333333"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8

            validator: RegExpValidator { regExp: /[a-zA-Z0-9_]{0,10}/ }

            background: Rectangle {
                color: "white"
                border.color: passwordField.activeFocus ? "#2196F3" : "#cccccc"
                border.width: passwordField.activeFocus ? 2 : 1
                radius: 4
            }
        }

        // 验证码输入框和验证码显示
        RowLayout {
            spacing: 10
            Layout.alignment: Qt.AlignHCenter

            TextField {
                id: captchaField
                placeholderText: "请输入验证码（区分大小写）"
                maximumLength: 4
                Layout.preferredWidth: 150
                Layout.preferredHeight: 40
                font.pointSize: 14
                color: "#333333"
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                rightPadding: 10
                topPadding: 8
                bottomPadding: 8

                background: Rectangle {
                    color: "white"
                    border.color: captchaField.activeFocus ? "#2196F3" : "#cccccc"
                    border.width: captchaField.activeFocus ? 2 : 1
                    radius: 4
                }
            }

            // 验证码显示区域（点击生成）
            Rectangle {
                id: captchaDisplay
                Layout.preferredWidth: 120
                Layout.preferredHeight: 40
                color: "#e0e0e0"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: loginViewModel.captchaCode.length > 0 ? loginViewModel.captchaCode : "点击获取"
                    font.pointSize: loginViewModel.captchaCode.length > 0 ? 18 : 12
                    font.bold: loginViewModel.captchaCode.length > 0
                    color: "#333333"
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: loginViewModel.generateCaptcha()
                }
            }
        }

        // 按钮区域
        RowLayout {
            spacing: 20
            Layout.alignment: Qt.AlignHCenter

            Button {
                id: loginButton
                text: "登录"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                font.pointSize: 16

                background: Rectangle {
                    color: "#4CAF50"
                    radius: 6
                }

                onClicked: {
                    if (!accountField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "用户名不能为空"
                        messageDialog.isLoginSuccess = false
                        messageDialog.open()
                        return
                    }
                    if (!passwordField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "密码不能为空"
                        messageDialog.isLoginSuccess = false
                        messageDialog.open()
                        return
                    }
                    if (!captchaField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "验证码不能为空"
                        messageDialog.isLoginSuccess = false
                        messageDialog.open()
                        return
                    }
                    loginViewModel.login(accountField.text, passwordField.text, captchaField.text)
                }
            }

            Button {
                id: registerButton
                text: "注册"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                font.pointSize: 16

                background: Rectangle {
                    color: "#2196F3"
                    radius: 6
                }

                onClicked: {
                    try {
                        var current = loginView
                        for (var i = 0; i < 10; i++) {
                            current = current.parent
                            if (current && current.push) {
                                current.push("RegisterView.qml")
                                break
                            }
                        }
                    } catch (e) {
                        console.log("跳转失败:", e)
                    }
                }
            }

            Button {
                id: cancelButton
                text: "取消"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                font.pointSize: 16

                background: Rectangle {
                    color: "#ff6b6b"
                    radius: 6
                }

                onClicked: {
                    try {
                        var current = loginView
                        for (var i = 0; i < 10; i++) {
                            current = current.parent
                            if (current && current.pop) {
                                current.pop()
                                break
                            }
                        }
                    } catch (e) {
                        console.log("返回失败:", e)
                    }
                }
            }
        }
    }
}
