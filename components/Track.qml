import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.trackBackground
    height: Theme.trackHeight //??

    property int trackNumber: 1
    property double pixelsPerSecond: 10
    property bool snapEnabled: true
    property var clips: [] // ← Получаем из C++!

    signal clipClicked(int clipId)
    signal clipDeleted(int clipId)
    signal clipDropped(string filepath, real time)
    signal clipMoved(int clipId, real newTime)

    // ===== ОБНОВИТЬ КЛИПЫ ИЗ C++ =====
    function updateClipsFromCpp() {
        console.log("🔄 Track.updateClipsFromCpp track:", root.trackNumber)

        // Получить клипы для этой дорожки из C++
        root.clips = cppTimeline.getClipsForTrack(root.trackNumber)

        console.log("   Получено клипов:", root.clips.length)
    }

    // ===== СЛУШАЕМ ИЗМЕНЕНИЯ ОТ C++ =====
    Connections {
        target: cppTimeline

        function onClipsChanged() {
            console.log("📡 Track", root.trackNumber, "получил clipsChanged")
            root.updateClipsFromCpp() // ← Обновляем!
        }
    }

    // ===== ИНИЦИАЛИЗАЦИЯ =====
    Component.onCompleted: {
        console.log("✅ Track", root.trackNumber, "создан")
        updateClipsFromCpp() // Загружаем клипы при создании
    }

    // Фоновые полосы
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.dividerColor
    }

    // Иконки дорожки
    Row {
        x: 10
        y: (root.height - height) / 2
        spacing: 5

        Rectangle {
            width: 20
            height: 20
            radius: 3
            color: Theme.rubyPrimary
            opacity: 0.3

            Text {
                anchors.centerIn: parent
                text: "V"
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.bold: true
            }
        }

        Rectangle {
            width: 20
            height: 20
            radius: 3
            color: Theme.rubyLight
            opacity: 0.3

            Text {
                anchors.centerIn: parent
                text: "A"
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    // Зона для Drag & Drop
    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["video/filepath"]

        onEntered: drag => {
                       dropHighlight.visible = true
                   }
        onExited: {
            dropHighlight.visible = false
        }
        onDropped: drop => {
                       dropHighlight.visible = false
                       var filepath = drop.getDataAsString("video/filepath")
                       var time = (drop.x) / root.pixelsPerSecond
                       console.log("🎬 DropArea drop:", filepath,
                                   "at time:", time)
                       root.clipDropped(filepath, time)
                   }
    }
    // Подсветка при наведении
    Rectangle {
        id: dropHighlight
        anchors.fill: parent
        color: Qt.rgba(Theme.rubyPrimary.r, Theme.rubyPrimary.g,
                       Theme.rubyPrimary.b, 0.15)
        border.color: Theme.rubyPrimary
        border.width: 2
        visible: false
    }

    // ===== REPEATER ДЛЯ КЛИПОВ (из C++!) =====
    Repeater {
        id: clipsRepeater
        model: root.clips // ← РЕАЛЬНЫЕ клипы из C++!

        delegate: VideoClip {
            id: clipItem

            // *** КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Qt.binding() делает привязки динамическими ***
            // Без этого x и width устанавливаются ОДИН РАЗ и не обновляются при зуме
            x: modelData.startTime * root.pixelsPerSecond
            y: 5
            width: modelData.duration * root.pixelsPerSecond
            height: root.height - 10

            clipName: modelData.filename
            clipId: modelData.id
            selected: modelData.selected || false
            thumbnailPath: modelData.thumbnailPath || ""

            Component.onCompleted: {
                console.log("🎬 VideoClip создан:", modelData.filename, "x:",
                            x, "width:", width)
            }

            // Привязка к изменению pixelsPerSecond — x и width обновляются при зуме
            Connections {
                target: root
                function onPixelsPerSecondChanged() {
                    clipItem.x = modelData.startTime * root.pixelsPerSecond
                    clipItem.width = modelData.duration * root.pixelsPerSecond
                }
            }

            onMoved: newX => {
                         var newTime = newX / root.pixelsPerSecond

                         // Snap к другим клипам
                         if (root.snapEnabled && root.clips) {
                             var snapThreshold = 0.5
                             for (var i = 0; i < root.clips.length; i++) {
                                 var otherClip = root.clips[i]
                                 if (otherClip.id === modelData.id)
                                 continue

                                 var otherStart = otherClip.startTime
                                 var otherEnd = otherClip.startTime + otherClip.duration

                                 if (Math.abs(
                                         newTime - otherStart) < snapThreshold) {
                                     newTime = otherStart
                                     break
                                 }
                                 if (Math.abs(
                                         newTime - otherEnd) < snapThreshold) {
                                     newTime = otherEnd
                                     break
                                 }
                                 var currentEnd = newTime + modelData.duration
                                 if (Math.abs(
                                         currentEnd - otherStart) < snapThreshold) {
                                     newTime = otherStart - modelData.duration
                                     break
                                 }
                             }
                         }

                         newTime = Math.max(0, newTime)
                         console.log("📍 Клип перемещён. ID:", modelData.id,
                                     "newTime:", newTime)
                         root.clipMoved(modelData.id, newTime)
                     }

            onClicked: {
                root.clipClicked(modelData.id)
            }

            onDeleteRequested: id => {
                                   console.log("🗑️ Delete requested for clip",
                                               id)
                                   cppTimeline.removeClip(id)
                                   root.clipDeleted(id)
                               }
        }
    }
}
