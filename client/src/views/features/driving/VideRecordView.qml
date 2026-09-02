import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3

//视频记录面板（被SettingsView的Loader加载）
Rectangle {
    id: videoRecordPanel
    color: "white"

    // 加载时从服务器获取视频列表
    Component.onCompleted: {
        if (videoRecordViewModel) {
            videoRecordViewModel.refreshVideoList()
        }
    }

    // 消息对话框
    MessageDialog {
        id: messageDialog
        title: "提示"
        text: ""
        icon: StandardIcon.Information
        standardButtons: StandardButton.Ok
    }

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
                text: "视频记录"
                color: "#333333"
                font.pointSize: 22
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Text {
                text: videoRecordViewModel ? "共 " + videoRecordViewModel.videoCount + " 个视频" : "共 0 个视频"
                color: "#999999"
                font.pointSize: 13
                Layout.alignment: Qt.AlignVCenter
            }

            // 刷新按钮
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 32
                text: "刷新"
                font.pointSize: 12

                background: Rectangle {
                    color: parent.hovered ? "#388E3C" : "#4CAF50"
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
                    if (videoRecordViewModel) {
                        videoRecordViewModel.refreshVideoList()
                    }
                }
            }
        }

        // 视频列表
        ListView {
            id: videoListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            clip: true

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            model: videoRecordViewModel ? videoRecordViewModel.videoListModel : null

            // 空列表提示
            Label {
                anchors.centerIn: parent
                visible: videoListView.count === 0
                text: "暂无视频记录\n录制视频后将自动显示"
                color: "#999999"
                font.pointSize: 16
                horizontalAlignment: Text.AlignHCenter
            }

            delegate: Rectangle {
                width: videoListView.width
                height: detailLoader.active ? 160 : 80
                color: mouseArea.containsMouse ? "#e8f5e9" : "#f9f9f9"
                radius: 8
                border.color: "#e0e0e0"
                border.width: 1

                Behavior on height {
                    NumberAnimation { duration: 200 }
                }

                // 存储当前项的videoId用于播放
                property int currentVideoId: model.videoId

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        detailLoader.active = !detailLoader.active
                    }
                }

                // 顶部简要信息（始终显示）
                RowLayout {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    height: 56
                    spacing: 12

                    // 视频图标
                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        color: "#4CAF50"
                        radius: 6

                        Text {
                            anchors.centerIn: parent
                            text: "\u25B6"
                            color: "white"
                            font.pointSize: 18
                        }
                    }

                    // 视频名称和时间
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            text: model.videoName
                            color: "#333333"
                            font.pointSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: model.videoDate
                            color: "#999999"
                            font.pointSize: 11
                        }
                    }

                    // 文件大小和时长
                    ColumnLayout {
                        spacing: 3
                        Layout.alignment: Qt.AlignRight

                        Text {
                            text: model.videoSize
                            color: "#666666"
                            font.pointSize: 12
                            Layout.alignment: Qt.AlignRight
                        }

                        Text {
                            text: model.videoDuration
                            color: "#666666"
                            font.pointSize: 12
                            Layout.alignment: Qt.AlignRight
                        }
                    }

                    // 播放记录标识
                    Rectangle {
                        visible: model.playCount > 0
                        Layout.preferredWidth: playCountTag.width + 16
                        Layout.preferredHeight: 24
                        color: "#E3F2FD"
                        radius: 12

                        Text {
                            id: playCountTag
                            anchors.centerIn: parent
                            text: "已看" + model.playCount + "次"
                            color: "#1976D2"
                            font.pointSize: 10
                        }
                    }

                    // 播放按钮
                    Button {
                        id: playBtn
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 36
                        text: "播放"
                        font.pointSize: 13

                        background: Rectangle {
                            color: playBtn.hovered ? "#388E3C" : "#4CAF50"
                            radius: 6
                        }

                        contentItem: Text {
                            text: playBtn.text
                            color: "white"
                            font.pointSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            openPlaybackWindow(model.videoName, model.filePath,
                                               model.videoDurationSec, model.lastPlaySec,
                                               model.videoId)
                        }
                    }

                    // 展开/收起详情指示
                    Text {
                        text: detailLoader.active ? "\u25B2" : "\u25BC"
                        color: "#999999"
                        font.pointSize: 10
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                // 详细信息区域（点击展开）
                Loader {
                    id: detailLoader
                    anchors.top: parent.top
                    anchors.topMargin: 58
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 70
                    anchors.rightMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    active: false

                    sourceComponent: Component {
                        RowLayout {
                            spacing: 20

                            // 成像数据
                            ColumnLayout {
                                spacing: 3

                                Text {
                                    text: "成像数据"
                                    color: "#333333"
                                    font.pointSize: 12
                                    font.bold: true
                                }
                                Text {
                                    text: "分辨率：" + model.resolution
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: "帧率：" + model.fps + "fps"
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: "摄像头：" + model.camera
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                            }

                            // 播放记录
                            ColumnLayout {
                                spacing: 3

                                Text {
                                    text: "播放记录"
                                    color: "#333333"
                                    font.pointSize: 12
                                    font.bold: true
                                }
                                Text {
                                    text: model.playCount > 0 ? ("上次播放到：" + model.lastPlayTime) : "未播放过"
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: "播放次数：" + model.playCount
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: model.playCount > 0 ? "续播将从上次位置开始" : ""
                                    color: "#1976D2"
                                    font.pointSize: 11
                                    visible: model.playCount > 0
                                }
                            }

                            // 文件信息
                            ColumnLayout {
                                spacing: 3

                                Text {
                                    text: "文件信息"
                                    color: "#333333"
                                    font.pointSize: 12
                                    font.bold: true
                                }
                                Text {
                                    text: "大小：" + model.videoSize
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: "时长：" + model.videoDuration
                                    color: "#666666"
                                    font.pointSize: 11
                                }
                                Text {
                                    text: "路径：" + model.filePath
                                    color: "#666666"
                                    font.pointSize: 10
                                    elide: Text.ElideMiddle
                                    Layout.maximumWidth: 200
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 打开视频播放独立窗口
    function openPlaybackWindow(videoName, filePath, durationSec, lastPlaySec, videoId) {
        // 使用qrc资源路径，VideoPlaybackWindow.qml已注册为views/VideoPlaybackWindow.qml
        var component = Qt.createComponent("qrc:/views/VideoPlaybackWindow.qml")
        if (component.status === Component.Ready) {
            var window = component.createObject(videoRecordPanel)
            window.videoTitle = videoName
            window.videoFilePath = filePath
            window.videoDurationSec = durationSec
            window.resumePositionSec = lastPlaySec
            window.videoId = videoId
            window.show()
        } else {
            console.log("创建播放窗口失败:", component.errorString())
        }
    }
}
