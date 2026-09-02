import QtQuick 2.12

// ============================================================================
// 统一线性图标组件
// 在 24x24 设计坐标系内用 Canvas 描边绘制，风格统一：
//   - 2px 线宽，圆角端点/连接
//   - 颜色由 color 属性控制，可随选中态动态变化
// 用法：LineIcon { name: "home"; color: "#ffffff"; size: 22 }
// 支持名称：home / video / car / reverse / birdview / search / settings
// ============================================================================
Canvas {
    id: root

    property string name: ""
    property color color: "#A5B9EF"
    property real size: 22

    width: size
    height: size
    antialiasing: true

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        // 把 24x24 设计坐标缩放到实际尺寸
        var s = width / 24
        ctx.save()
        ctx.scale(s, s)
        ctx.strokeStyle = color
        ctx.fillStyle = color
        ctx.lineWidth = 2
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        switch (name) {
            case "home":      drawHome(ctx); break
            case "video":     drawVideo(ctx); break
            case "car":       drawCar(ctx); break
            case "reverse":   drawReverse(ctx); break
            case "birdview":  drawBirdview(ctx); break
            case "search":    drawSearch(ctx); break
            case "settings":  drawSettings(ctx); break
        }
        ctx.restore()
    }

    // 主页：屋顶 + 墙体 + 门
    function drawHome(ctx) {
        ctx.beginPath()
        ctx.moveTo(3, 11); ctx.lineTo(12, 4); ctx.lineTo(21, 11)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(5, 9.5); ctx.lineTo(5, 20); ctx.lineTo(19, 20); ctx.lineTo(19, 9.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(10, 20); ctx.lineTo(10, 15); ctx.lineTo(14, 15); ctx.lineTo(14, 20)
        ctx.stroke()
    }

    // 视频记录：机身 + 镜头
    function drawVideo(ctx) {
        ctx.beginPath()
        ctx.rect(3, 8, 11, 8)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(14, 10); ctx.lineTo(20, 7.5); ctx.lineTo(20, 16.5); ctx.lineTo(14, 14)
        ctx.closePath()
        ctx.stroke()
    }

    // 行车模式：车辆侧视轮廓 + 两轮
    function drawCar(ctx) {
        ctx.beginPath()
        ctx.moveTo(2.5, 14)
        ctx.lineTo(5, 14)
        ctx.lineTo(7, 10)
        ctx.lineTo(15, 10)
        ctx.lineTo(17, 14)
        ctx.lineTo(21.5, 14)
        ctx.lineTo(21.5, 16.5)
        ctx.lineTo(2.5, 16.5)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath(); ctx.arc(7, 16.5, 1.6, 0, Math.PI * 2); ctx.stroke()
        ctx.beginPath(); ctx.arc(17, 16.5, 1.6, 0, Math.PI * 2); ctx.stroke()
    }

    // 倒车模式：U形掉头箭头
    function drawReverse(ctx) {
        ctx.beginPath()
        ctx.moveTo(8, 18)
        ctx.lineTo(8, 12)
        ctx.arc(12, 12, 4, Math.PI, 2 * Math.PI, false)   // 经顶部到右侧
        ctx.lineTo(16, 18)
        ctx.stroke()
        // 末端向下箭头
        ctx.beginPath()
        ctx.moveTo(16, 18); ctx.lineTo(13.5, 15)
        ctx.moveTo(16, 18); ctx.lineTo(18.5, 15)
        ctx.stroke()
    }

    // 俯视模式：环形十字准星
    function drawBirdview(ctx) {
        ctx.beginPath(); ctx.arc(12, 12, 8, 0, Math.PI * 2); ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(12, 2); ctx.lineTo(12, 6)
        ctx.moveTo(12, 18); ctx.lineTo(12, 22)
        ctx.moveTo(2, 12); ctx.lineTo(6, 12)
        ctx.moveTo(18, 12); ctx.lineTo(22, 12)
        ctx.stroke()
        ctx.beginPath(); ctx.arc(12, 12, 1.5, 0, Math.PI * 2); ctx.stroke()
    }

    // 特征记录：放大镜
    function drawSearch(ctx) {
        ctx.beginPath(); ctx.arc(10, 10, 6, 0, Math.PI * 2); ctx.stroke()
        ctx.beginPath(); ctx.moveTo(14.3, 14.3); ctx.lineTo(20, 20); ctx.stroke()
    }

    // 系统设置：齿轮（8齿 + 外环 + 中心孔）
    function drawSettings(ctx) {
        for (var i = 0; i < 8; i++) {
            var a = i * Math.PI / 4
            ctx.beginPath()
            ctx.moveTo(12 + Math.cos(a) * 7, 12 + Math.sin(a) * 7)
            ctx.lineTo(12 + Math.cos(a) * 9.5, 12 + Math.sin(a) * 9.5)
            ctx.stroke()
        }
        ctx.beginPath(); ctx.arc(12, 12, 6, 0, Math.PI * 2); ctx.stroke()
        ctx.beginPath(); ctx.arc(12, 12, 2.5, 0, Math.PI * 2); ctx.stroke()
    }
}
