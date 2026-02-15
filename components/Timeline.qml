import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.timelineBackground

    property real currentTime: 0
    property real duration: 100  // Минимум 100 секунд
    property int zoomLevel: 100
    property real pixelsPerSecond: 10
    property var clipManager: null
    property bool snapEnabled: true  // Snap включён

    signal timeChanged(real time)
    signal zoomChanged(int zoom)

    onZoomLevelChanged: {
        pixelsPerSecond = zoomLevel / 10
    }
    
    // Автоматически расширяем duration когда добавляются клипы
    Connections {
        target: clipManager
        function onRevisionChanged() {
            updateDuration()
        }
    }
    
    function updateDuration() {
        if (!clipManager) return
        
        // Находим максимальное endTime всех клипов
        var maxEndTime = 100  // Минимум
        
        for (var i = 0; i < clipManager.clips.length; i++) {
            var clip = clipManager.clips[i]
            var endTime = clip.startTime + clip.duration
            if (endTime > maxEndTime) {
                maxEndTime = endTime
            }
        }
        
        // Добавляем буфер 20 секунд
        duration = maxEndTime + 20
        console.log("Timeline duration обновлён:", duration, "сек")
    }
    
    Component.onCompleted: {
        updateDuration()
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Левая панель с номерами дорожек
        Rectangle {
            Layout.preferredWidth: 120
            Layout.fillHeight: true
            color: Theme.panelBackground

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.dividerColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Заголовок
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Theme.backgroundDark

                    Text {
                        anchors.centerIn: parent
                        text: "ДОРОЖКИ"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.dividerColor
                    }
                }

                // Дорожки - 3 штуки
                Repeater {
                    model: 2

                    TrackLabel {
                        trackNumber: index + 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.trackHeight * 2
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Область таймлайна - РАСТЯНУТА
        Flickable {
            id: timelineFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: Math.max(width, root.duration * root.pixelsPerSecond + 100)
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AlwaysOn
                
                contentItem: Rectangle {
                    implicitHeight: 8
                    radius: 4
                    color: Theme.rubyPrimary
                    opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                }
            }

            // Масштабирование колесиком мыши
            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                acceptedButtons: Qt.NoButton
                
                onWheel: (wheel) => {
                    if (wheel.modifiers & Qt.ControlModifier) {
                        var delta = wheel.angleDelta.y > 0 ? 10 : -10
                        var newZoom = Math.max(20, Math.min(500, root.zoomLevel + delta))
                        root.zoomChanged(newZoom)
                        wheel.accepted = true
                    } else {
                        wheel.accepted = false
                    }
                }
            }

            ColumnLayout {
                width: Math.max(timelineFlickable.width, timelineFlickable.contentWidth)
                height: timelineFlickable.height
                spacing: 0

                // Временная шкала
                TimeRuler {
                    id: timeRuler
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    duration: root.duration
                    pixelsPerSecond: root.pixelsPerSecond
                    currentTime: root.currentTime
                }

                // Дорожки - 3 штуки
                Repeater {
                    model: 2

                    Track {
                        trackNumber: index + 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.trackHeight * 2
                        duration: root.duration
                        pixelsPerSecond: root.pixelsPerSecond
                        currentTime: root.currentTime
                        snapEnabled: root.snapEnabled  // Передаём snap!
                        clipManager: root.clipManager
                        
                        // Когда видео дропнули
                        onClipDropped: (filepath, time) => {
                            console.log("=== Timeline.onClipDropped ===")
                            console.log("filepath:", filepath)
                            console.log("time:", time)
                            console.log("track:", index + 1)
                            console.log("clipManager exists:", !!root.clipManager)
                            if (root.clipManager) {
                                console.log("Calling clipManager.addClip...")
                                root.clipManager.addClip(filepath, index + 1, time, 10.0)
                            } else {
                                console.log("ERROR: clipManager is NULL!")
                            }
                        }
                        
                        // Когда клип подвинули
                        onClipMoved: (clipId, newTime) => {
                            if (root.clipManager) {
                                root.clipManager.moveClip(clipId, index + 1, newTime)
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Линия воспроизведения (Playhead)
            Rectangle {
                id: playhead
                x: root.currentTime * root.pixelsPerSecond
                y: 0
                width: 2
                height: timelineFlickable.contentHeight
                color: Theme.rubyPrimary
                z: 1000

                // Треугольник сверху
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -10
                    width: 14
                    height: 22
                    color: Theme.rubyPrimary
                    radius: 2

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.fillStyle = Theme.rubyPrimary
                            ctx.beginPath()
                            ctx.moveTo(7, 22)
                            ctx.lineTo(0, 16)
                            ctx.lineTo(14, 16)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }
                }

                // Временной код на playhead
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - height - 5
                    width: playheadTime.width + 10
                    height: playheadTime.height + 6
                    color: Qt.rgba(0, 0, 0, 0.9)
                    radius: 3
                    border.color: Theme.rubyPrimary
                    border.width: 1

                    Text {
                        id: playheadTime
                        anchors.centerIn: parent
                        text: formatTime(root.currentTime)
                        color: Theme.rubyLight
                        font.family: "Consolas, monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Behavior on x {
                    enabled: !playheadMouseArea.drag.active
                    NumberAnimation { duration: 100 }
                }
            }

            // MouseArea для перемещения playhead
            MouseArea {
                id: playheadMouseArea
                anchors.fill: parent
                z: 999
                acceptedButtons: Qt.LeftButton
                
                onClicked: (mouse) => {
                    var time = (mouse.x + timelineFlickable.contentX) / root.pixelsPerSecond
                    root.timeChanged(Math.max(0, Math.min(root.duration, time)))
                }
            }
        }
    }

    function formatTime(seconds) {
        var hours = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = Math.floor(seconds % 60)
        var frames = Math.floor((seconds % 1) * 30)
        
        if (hours > 0) {
            return pad(hours) + ":" + pad(mins) + ":" + pad(secs)
        }
        return pad(mins) + ":" + pad(secs) + ":" + pad(frames, 2)
    }

    function pad(num, digits = 2) {
        return String(num).padStart(digits, '0')
    }

    component TrackLabel: Rectangle {
        property int trackNumber: 1
        color: Theme.panelBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.dividerColor
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacingSmall

            Text {
                text: "Дорожка " + trackNumber
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.alignment: Qt.AlignHCenter

                Rectangle {
                    width: 18
                    height: 18
                    radius: 2
                    color: Theme.clipColor

                    Text {
                        anchors.centerIn: parent
                        text: "V"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Rectangle {
                    width: 18
                    height: 18
                    radius: 2
                    color: Theme.waveformColor

                    Text {
                        anchors.centerIn: parent
                        text: "A"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }
    }
}
