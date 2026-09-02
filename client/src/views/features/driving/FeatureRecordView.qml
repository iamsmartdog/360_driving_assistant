import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// 特征记录面板 - 展示从服务器获取的特征截图记录
Rectangle {
    id: featureRecordPanel
    color: "white"

    // 当前查看的大图路径
    property string currentImagePath: ""
    property string currentImageName: ""

    // 使用StackView管理列表↔详情切换
    StackView {
        id: featureStackView
        anchors.fill: parent
        initialItem: screenshotListPage
    }

    // 截图列表页
    Component {
        id: screenshotListPage

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 30
            anchors.rightMargin: 30
            anchors.topMargin: 20
            anchors.bottomMargin: 20
            spacing: 10

            // 标题栏
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Text {
                    text: "\u7279\u5F81\u8BB0\u5F55"
                    color: "#333333"
                    font.pointSize: 22
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    text: "\u5237\u65B0"
                    font.pointSize: 13

                    background: Rectangle {
                        color: parent.hovered ? "#388E3C" : "#4CAF50"
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
                        if (screenshotListViewModel) {
                            screenshotListViewModel.refreshScreenshotList()
                        }
                    }
                }

                Text {
                    text: "\u5171 " + (screenshotListViewModel ? screenshotListViewModel.screenshotCount : 0) + " \u6761\u8BB0\u5F55"
                    color: "#999999"
                    font.pointSize: 13
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            // 截图列表
            ListView {
                id: screenshotListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                clip: true

                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                }

                model: screenshotListViewModel ? screenshotListViewModel.screenshotListModel : null

                delegate: Rectangle {
                    width: screenshotListView.width
                    height: 80
                    color: mouseArea.containsMouse ? "#e8f5e9" : "#f9f9f9"
                    radius: 8
                    border.color: "#e0e0e0"
                    border.width: 1

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: {
                            currentImagePath = model.screenshotPath
                            currentImageName = model.screenshotName
                            featureStackView.push(screenshotDetailPage)
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        // 截图图标
                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 50
                            color: "#FF9800"
                            radius: 6

                            Text {
                                anchors.centerIn: parent
                                text: "\uD83D\uDCF7"
                                font.pointSize: 20
                            }
                        }

                        // 文件名和日期
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: model.screenshotName
                                color: "#333333"
                                font.pointSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: model.createdAt
                                color: "#999999"
                                font.pointSize: 11
                            }

                            Text {
                                text: model.detectionInfo || "\u65E0\u68C0\u6D4B\u4FE1\u606F"
                                color: "#666666"
                                font.pointSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // 类型和大小
                        ColumnLayout {
                            spacing: 3
                            Layout.alignment: Qt.AlignRight

                            Rectangle {
                                Layout.preferredWidth: typeTag.width + 12
                                Layout.preferredHeight: 22
                                color: model.recordType === "\u884C\u8F66\u622A\u56FE" ? "#E3F2FD" :
                                       model.recordType === "\u64AD\u653E\u622A\u56FE" ? "#FFF3E0" :
                                       model.recordType === "\u5012\u8F66\u622A\u56FE" ? "#FCE4EC" : "#F5F5F5"
                                radius: 11

                                Text {
                                    id: typeTag
                                    anchors.centerIn: parent
                                    text: model.recordType || "\u672A\u77E5"
                                    color: model.recordType === "\u884C\u8F66\u622A\u56FE" ? "#1976D2" :
                                           model.recordType === "\u64AD\u653E\u622A\u56FE" ? "#E65100" :
                                           model.recordType === "\u5012\u8F66\u622A\u56FE" ? "#C2185B" : "#666666"
                                    font.pointSize: 10
                                }
                            }

                            Text {
                                text: model.screenshotSizeKB + " KB"
                                color: "#666666"
                                font.pointSize: 11
                                Layout.alignment: Qt.AlignRight
                            }
                        }
                    }
                }

                // 空状态提示
                Label {
                    anchors.centerIn: parent
                    visible: screenshotListView.count === 0
                    text: "\u6682\u65E0\u7279\u5F81\u8BB0\u5F55\n\u70B9\u51FB\"\u5237\u65B0\"\u4ECE\u670D\u52A1\u5668\u83B7\u53D6"
                    color: "#999999"
                    font.pointSize: 14
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // 截图详情/放大查看页
    Component {
        id: screenshotDetailPage

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 顶部导航栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: "#f5f5f5"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15

                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 36
                        text: "\u2190 \u8FD4\u56DE"

                        background: Rectangle {
                            color: parent.hovered ? "#e0e0e0" : "#dddddd"
                            radius: 6
                        }

                        contentItem: Text {
                            text: parent.text
                            color: "#333333"
                            font.pointSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            featureStackView.pop()
                        }
                    }

                    Text {
                        text: currentImageName
                        color: "#333333"
                        font.pointSize: 16
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            // 图片展示区域
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#2a2a2a"

                Text {
                    anchors.centerIn: parent
                    text: "\u7279\u5F81\u56FE\u7247\u9884\u89C8\n" + currentImageName
                    color: "#888888"
                    font.pointSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Image {
                    anchors.fill: parent
                    anchors.margins: 20
                    fillMode: Image.PreserveAspectFit
                    source: currentImagePath !== "" ? "file://" + currentImagePath : ""
                    visible: currentImagePath !== "" && status === Image.Ready

                    onStatusChanged: {
                        if (status === Image.Error) {
                            console.log("\u56FE\u7247\u52A0\u8F7D\u5931\u8D25:", currentImagePath)
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (screenshotListViewModel) {
            screenshotListViewModel.refreshScreenshotList()
        }
    }
}
