import QtQuick 2.12
import QtQuick.Controls 2.12

/**
 * @brief 自定义消息对话框组件
 * 用于显示成功、错误、警告等提示信息
 */
Popup {
    id: messageDialog
    anchors.centerIn: parent
    width: 400
    height: 200
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // 消息类型
    property string messageType: "info"  // "success", "error", "warning", "info"

    // 消息标题
    property string messageTitle: ""

    // 消息内容
    property string messageContent: ""

    // 点击确认按钮的回调
    signal confirmed()

    // 背景遮罩
    background: Rectangle {
        color: "#80000000"
    }

    // 对话框主体
    contentItem: Rectangle {
        color: "white"
        radius: 10
        border.width: 2
        border.color: {
            switch(messageDialog.messageType) {
                case "success": return "#4CAF50"
                case "error": return "#F44336"
                case "warning": return "#FF9800"
                default: return "#2196F3"
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            // 标题
            Text {
                text: messageDialog.messageTitle
                font.pointSize: 18
                font.bold: true
                color: {
                    switch(messageDialog.messageType) {
                        case "success": return "#4CAF50"
                        case "error": return "#F44336"
                        case "warning": return "#FF9800"
                        default: return "#2196F3"
                    }
                }
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            // 内容
            Text {
                text: messageDialog.messageContent
                font.pointSize: 14
                color: "#333333"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.fillHeight: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            // 确认按钮
            Button {
                text: "确定"
                font.pointSize: 16
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                Layout.preferredHeight: 40

                background: Rectangle {
                    color: parent.down ? "#1976D2" : (parent.hovered ? "#42A5F5" : "#2196F3")
                    radius: 5
                }

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    messageDialog.confirmed()
                    messageDialog.close()
                }
            }
        }
    }

    // 便捷方法：显示成功消息
    function showSuccess(title, content) {
        messageType = "success"
        messageTitle = title
        messageContent = content
        open()
    }

    // 便捷方法：显示错误消息
    function showError(title, content) {
        messageType = "error"
        messageTitle = title
        messageContent = content
        open()
    }

    // 便捷方法：显示警告消息
    function showWarning(title, content) {
        messageType = "warning"
        messageTitle = title
        messageContent = content
        open()
    }

    // 便捷方法：显示信息消息
    function showInfo(title, content) {
        messageType = "info"
        messageTitle = title
        messageContent = content
        open()
    }
}