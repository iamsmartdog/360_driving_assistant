import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//注册界面（使用Rectangle，适配StackView）
Rectangle {
    id: registerView
    color: "#f5f5f5"

    // 消息对话框
    MessageDialog {
        id: messageDialog
        title: "提示"
        text: ""
        icon: StandardIcon.Information
        standardButtons: StandardButton.Ok
        property bool isRegisterSuccess: false
        onAccepted: {
            if (isRegisterSuccess) {
                // 注册成功，返回登录界面
                try {
                    var current = registerView
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

    // 监听注册结果
    Connections {
        target: loginViewModel
        onRegisterSuccess: {
            messageDialog.icon = StandardIcon.Information
            messageDialog.title = "注册成功"
            messageDialog.text = "您的账号为：" + loginViewModel.currentAccount + "\n请牢记此账号，用于登录。"
            messageDialog.isRegisterSuccess = true
            messageDialog.open()
        }
        onRegisterFailed: {
            messageDialog.icon = StandardIcon.Critical
            messageDialog.title = "注册失败"
            messageDialog.text = errorMessage
            messageDialog.isRegisterSuccess = false
            messageDialog.open()
        }
        onValidationError: {
            messageDialog.icon = StandardIcon.Warning
            messageDialog.title = "输入错误"
            messageDialog.text = errorMessage
            messageDialog.isRegisterSuccess = false
            messageDialog.open()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        // 标题
        Text {
            text: "360度智能行车辅助系统"
            color: "#333333"
            font.pointSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: "注册后系统将自动分配11位账号"
            color: "#666666"
            font.pointSize: 12
            Layout.alignment: Qt.AlignHCenter
        }

        // 昵称输入框
        TextField {
            id: nicknameField
            placeholderText: "请输入昵称"
            maximumLength: 12
            Layout.preferredWidth: 300
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            font.pointSize: 14
            color: "#333333"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8

            background: Rectangle {
                color: "white"
                border.color: nicknameField.activeFocus ? "#2196F3" : "#cccccc"
                border.width: nicknameField.activeFocus ? 2 : 1
                radius: 4
            }
        }

        // 密码输入框
        TextField {
            id: passwordField
            placeholderText: "请输入密码（6~10位字母、数字、下划线）"
            echoMode: TextInput.Password
            maximumLength: 10
            Layout.preferredWidth: 300
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            font.pointSize: 14
            color: "#333333"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8

            background: Rectangle {
                color: "white"
                border.color: passwordField.activeFocus ? "#2196F3" : "#cccccc"
                border.width: passwordField.activeFocus ? 2 : 1
                radius: 4
            }
        }

        // 确认密码输入框
        TextField {
            id: confirmPasswordField
            placeholderText: "请确认密码"
            echoMode: TextInput.Password
            maximumLength: 10
            Layout.preferredWidth: 300
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            font.pointSize: 14
            color: "#333333"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8

            background: Rectangle {
                color: "white"
                border.color: confirmPasswordField.activeFocus ? "#2196F3" : "#cccccc"
                border.width: confirmPasswordField.activeFocus ? 2 : 1
                radius: 4
            }
        }

        // 按钮区域
        RowLayout {
            spacing: 20
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: "注册"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                font.pointSize: 16

                background: Rectangle {
                    color: "#4CAF50"
                    radius: 6
                }

                onClicked: {
                    if (!nicknameField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "昵称不能为空"
                        messageDialog.isRegisterSuccess = false
                        messageDialog.open()
                        return
                    }
                    if (!passwordField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "密码不能为空"
                        messageDialog.isRegisterSuccess = false
                        messageDialog.open()
                        return
                    }
                    if (passwordField.text !== confirmPasswordField.text) {
                        messageDialog.icon = StandardIcon.Warning
                        messageDialog.title = "提示"
                        messageDialog.text = "两次输入的密码不一致"
                        messageDialog.isRegisterSuccess = false
                        messageDialog.open()
                        return
                    }
                    loginViewModel.registerUser(nicknameField.text, passwordField.text, nicknameField.text)
                }
            }

            Button {
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
                        var current = registerView
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
