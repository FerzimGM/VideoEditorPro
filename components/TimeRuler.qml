import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.backgroundDark

    property real duration: 100
    property real pixelsPerSecond: 10
    property real currentTime: 0

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.dividerColor
    }

    Canvas {
        id: rulerCanvas
        anchors.fill: parent
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            
            // Адаптивный интервал - УЛУЧШЕННЫЙ
            var secondsPerMark = 1
            
            if (pixelsPerSecond < 2) {
                secondsPerMark = 60  // Каждую минуту
            } else if (pixelsPerSecond < 5) {
                secondsPerMark = 30  // Каждые 30 секунд
            } else if (pixelsPerSecond < 10) {
                secondsPerMark = 10  // Каждые 10 секунд
            } else if (pixelsPerSecond < 20) {
                secondsPerMark = 5   // Каждые 5 секунд
            } else if (pixelsPerSecond < 40) {
                secondsPerMark = 2   // Каждые 2 секунды
            } else {
                secondsPerMark = 1   // Каждую секунду
            }
            
            for (var time = 0; time <= duration; time += secondsPerMark) {
                var x = time * pixelsPerSecond
                
                ctx.strokeStyle = Theme.textSecondary
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(x, height - 20)
                ctx.lineTo(x, height)
                ctx.stroke()
                
                // Текст времени
                ctx.fillStyle = Theme.textPrimary
                ctx.font = "bold 11px 'Segoe UI', Arial, sans-serif"
                var timeText = formatTime(time)
                var textWidth = ctx.measureText(timeText).width
                ctx.fillText(timeText, x - textWidth / 2, height - 25)
                
                // Промежуточные метки (половинные интервалы)
                if (pixelsPerSecond >= 15 && secondsPerMark > 1) {
                    var halfTime = time + secondsPerMark / 2
                    if (halfTime <= duration) {
                        var halfX = halfTime * pixelsPerSecond
                        ctx.strokeStyle = Theme.textDisabled
                        ctx.globalAlpha = 0.5
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(halfX, height - 10)
                        ctx.lineTo(halfX, height)
                        ctx.stroke()
                        ctx.globalAlpha = 1.0
                    }
                }
            }
        }
        
        Connections {
            target: root
            function onPixelsPerSecondChanged() { rulerCanvas.requestPaint() }
            function onDurationChanged() { rulerCanvas.requestPaint() }
        }
    }

    function formatTime(seconds) {
        var hours = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = Math.floor(seconds % 60)
        
        if (hours > 0) {
            return pad(hours) + ":" + pad(mins) + ":" + pad(secs)
        } else {
            return pad(mins) + ":" + pad(secs)
        }
    }

    function pad(num) {
        return num < 10 ? "0" + num : num
    }
}
