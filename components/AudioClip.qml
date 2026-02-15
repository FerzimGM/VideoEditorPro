import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: isPrimary ? Qt.lighter(Theme.waveformColor, 1.1) : Theme.waveformColor
    radius: Theme.borderRadius
    border.color: selected ? Theme.rubyPrimary : (isPrimary ? Theme.rubyDark : Qt.darker(Theme.waveformColor, 1.2))
    border.width: selected ? 2 : (isPrimary ? 2 : 1)
    clip: true

    property string clipName: "Audio Clip"
    property int trackNumber: 1
    property bool isPrimary: false
    property real pixelsPerSecond: 10
    property bool selected: false
    property bool snapEnabled: true
    property real snapThreshold: 5

    // Градиент
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: isPrimary ? Theme.rubyGradientStart : Qt.lighter(Theme.waveformColor, 1.2) }
            GradientStop { position: 0.5; color: isPrimary ? Qt.rgba(0, 0, 0, 0) : Qt.rgba(0, 0, 0, 0) }
            GradientStop { position: 1.0; color: isPrimary ? Theme.rubyGradientEnd : Qt.darker(Theme.waveformColor, 1.1) }
        }
        opacity: isPrimary ? 0.2 : 0.5
    }

    // ===== WAVEFORM CANVAS =====
    // TODO: FFmpeg will extract audio samples
    // cppFFmpeg.extractWaveform(audioPath) → signal waveformReady(samples[])
    Canvas {
        id: waveformCanvas
        anchors.fill: parent
        anchors.margins: 2
        
        property var waveformData: generateWaveformData()  // Mock data
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            
            ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.3)
            ctx.fillStyle = Qt.rgba(0, 0, 0, 0.2)
            ctx.lineWidth = 1
            
            var centerY = height / 2
            var pointsPerPixel = Math.max(1, Math.floor(waveformData.length / width))
            
            ctx.beginPath()
            ctx.moveTo(0, centerY)
            
            // Рисуем верхнюю часть волны
            for (var x = 0; x < width; x++) {
                var dataIndex = Math.floor(x * pointsPerPixel)
                if (dataIndex < waveformData.length) {
                    var amplitude = waveformData[dataIndex]
                    var y = centerY - (amplitude * (height / 2) * 0.8)
                    ctx.lineTo(x, y)
                }
            }
            
            // Рисуем нижнюю часть волны
            for (var x = width - 1; x >= 0; x--) {
                var dataIndex = Math.floor(x * pointsPerPixel)
                if (dataIndex < waveformData.length) {
                    var amplitude = waveformData[dataIndex]
                    var y = centerY + (amplitude * (height / 2) * 0.8)
                    ctx.lineTo(x, y)
                }
            }
            
            ctx.closePath()
            ctx.fill()
            ctx.stroke()
        }
        
        // Mock waveform generator (пока нет FFmpeg)
        function generateWaveformData() {
            // TODO: Replace with real audio samples from FFmpeg
            var data = []
            var points = 200
            for (var i = 0; i < points; i++) {
                var t = i / points
                var amplitude = Math.sin(t * Math.PI * 4) * 0.5 + 
                               Math.sin(t * Math.PI * 8) * 0.3 + 
                               Math.random() * 0.2
                data.push(Math.abs(amplitude))
            }
            return data
        }
        
        Connections {
            target: root
            function onWidthChanged() { waveformCanvas.requestPaint() }
        }
        
        // TODO: Подключение к FFmpeg
        // Connections {
        //     target: cppFFmpeg
        //     function onWaveformReady(filepath, samples) {
        //         if (filepath === root.audioPath) {
        //             waveformData = samples
        //             requestPaint()
        //         }
        //     }
        // }
    }

    // Информация о клипе
    RowLayout {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 4
        spacing: 4

        Text {
            text: root.clipName
            color: "#FFFFFF"
            font.family: Theme.fontFamily
            font.pixelSize: 10
            font.bold: true
        }

        Text {
            text: isPrimary ? "★" : ""
            color: Theme.rubyLight
            font.pixelSize: 10
            visible: isPrimary
        }
    }

    // Иконка аудио
    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 4
        text: "🎵"
        font.pixelSize: 12
    }

    // Drag & Drop
    Drag.active: dragArea.drag.active
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2

    MouseArea {
        id: dragArea
        anchors.fill: parent
        drag.target: parent
        drag.axis: Drag.XAxis
        hoverEnabled: true
        
        property real startX: 0
        
        onPressed: (mouse) => {
            root.selected = true
            startX = root.x
            root.z = 100
        }
        
        onReleased: {
            root.z = 1
            
            if (root.snapEnabled) {
                var snapX = findSnapPosition()
                if (snapX !== null) {
                    root.x = snapX
                }
            }
            
            if (root.x < 0) root.x = 0
            var maxX = root.parent.width - root.width
            if (root.x > maxX) root.x = maxX
        }
        
        onDoubleClicked: {
            console.log("Audio clip double-clicked:", root.clipName)
        }
    }

    // Handles для изменения длительности
    Rectangle {
        id: leftHandle
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 8
        color: handleMouseArea.containsMouse ? Theme.rubyLight : Theme.rubyPrimary
        visible: root.selected
        
        MouseArea {
            id: handleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            
            onPressed: (mouse) => {
                mouse.accepted = true
            }
            
            onPositionChanged: (mouse) => {
                if (pressed) {
                    var newWidth = root.width - mouse.x
                    if (newWidth >= 20) {
                        root.x += mouse.x
                        root.width = newWidth
                        waveformCanvas.requestPaint()
                    }
                }
            }
        }
    }

    Rectangle {
        id: rightHandle
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 8
        color: rightHandleMouseArea.containsMouse ? Theme.rubyLight : Theme.rubyPrimary
        visible: root.selected
        
        MouseArea {
            id: rightHandleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            
            onPressed: (mouse) => {
                mouse.accepted = true
            }
            
            onPositionChanged: (mouse) => {
                if (pressed) {
                    var newWidth = mouse.x
                    if (newWidth >= 20) {
                        root.width = newWidth
                        waveformCanvas.requestPaint()
                    }
                }
            }
        }
    }

    // Эффект выделения
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.color: Theme.rubyLight
        border.width: 2
        visible: root.selected
        opacity: 0.5
    }

    function findSnapPosition() {
        return null
    }
}
