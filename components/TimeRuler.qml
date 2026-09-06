import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

/**
 * TimeRuler
 * ---------
 * Time ruler shown above the timeline: draws tick marks and time labels
 * using QML Canvas (low-level 2D-context drawing rather than a set of
 * Item elements — cheaper for dozens/hundreds of marks). The spacing
 * between ticks is adaptive and depends on the current zoom level
 * (pixelsPerSecond), so ticks don't overlap when zoomed out and aren't
 * too sparse when zoomed in.
 */
Rectangle {
    id: root
    color: Theme.backgroundDark

    property real duration: 100        // total timeline duration, seconds — defines the drawing range
    property real pixelsPerSecond: 10  // current timeline zoom level; drives tick spacing
    property real currentTime: 0       // current playback position (not used directly in ruler drawing)

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

            // Adaptive spacing between major ticks: the fewer pixels per
            // second (further zoomed out), the sparser the ticks — otherwise
            // time labels would overlap at small zoom levels
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

            // Main draw loop: one tick + time label per secondsPerMark step,
            // up to the end of the timeline duration
            for (var time = 0; time <= duration; time += secondsPerMark) {
                var x = time * pixelsPerSecond

                ctx.strokeStyle = Theme.textSecondary
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(x, height - 20)
                ctx.lineTo(x, height)
                ctx.stroke()

                // Time label (centered on the tick via measureText)
                ctx.fillStyle = Theme.textPrimary
                ctx.font = "bold 11px 'Segoe UI', Arial, sans-serif"
                var timeText = formatTime(time)
                var textWidth = ctx.measureText(timeText).width
                ctx.fillText(timeText, x - textWidth / 2, height - 25)

                // Intermediate (half-step) marks are drawn only when zoomed
                // in enough and the main interval is bigger than one second —
                // otherwise there's nothing meaningful to subdivide
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

        // Canvas does not repaint automatically on property changes —
        // explicitly request a repaint when zoom or duration changes
        Connections {
            target: root
            function onPixelsPerSecondChanged() { rulerCanvas.requestPaint() }
            function onDurationChanged() { rulerCanvas.requestPaint() }
        }
    }

    // Formats time as HH:MM:SS, or MM:SS when there are no hours —
    // avoids a redundant leading "00:" for short clips/projects
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

    // Zero-pads a number to two digits (05, 12, ...)
    function pad(num) {
        return num < 10 ? "0" + num : num
    }
}
