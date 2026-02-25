import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.trackBackground
    // Высота = видео-полоска (50) + аудио-полоска (30) + зазор (2) + отступы (10)
    // Должна совпадать с VideoClip.videoH + VideoClip.audioH + 2
    // Theme.trackHeight * 2 из Timeline.qml — задаётся снаружи, не трогаем
    height: Theme.trackHeight * 2

    property int trackNumber: 1
    property double pixelsPerSecond: 10
    property bool snapEnabled: true
    property var clips: []
    // *** Получаем от Timeline, передаём в VideoClip ***
    property int selectedClipId: -1

    signal clipClicked(int clipId)
    signal clipSelected(int clipId)
    // ← новый: пробрасывается вверх в Timeline
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

    // Зона Drag & Drop
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
                       var time = drop.x / root.pixelsPerSecond
                       console.log("🎬 Drop:", filepath, "at", time, "sec")
                       root.clipDropped(filepath, time)
                   }
    }

    // Подсветка при drag-over
    Rectangle {
        id: dropHighlight
        anchors.fill: parent
        color: Qt.rgba(Theme.rubyPrimary.r, Theme.rubyPrimary.g,
                       Theme.rubyPrimary.b, 0.15)
        border.color: Theme.rubyPrimary
        border.width: 2
        visible: false
        z: 5
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
            y: (root.height - height) / 2 // вертикальное центрирование
            width: modelData.duration * root.pixelsPerSecond

            clipName: modelData.filename
            clipId: modelData.id
            // *** selected привязан к selectedClipId от Track ***
            selected: root.selectedClipId === modelData.id
            isMuted: modelData.isMuted || false

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

            // ─── СИГНАЛЫ ОТ VideoClip ─────────────────────────────────
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
                // *** Выделяем клип через сигнал вверх по цепочке ***
                root.clipSelected(modelData.id)
                root.clipClicked(modelData.id)
            }

            onDeleteRequested: id => {
                                   console.log("🗑️ Delete clip", id)
                                   // Снимаем выделение если удаляем выделенный
                                   if (root.selectedClipId === id)
                                   root.clipSelected(-1)
                                   cppTimeline.removeClip(id)
                                   root.clipDeleted(id)
                               }
            onSplitRequested: id => {
                                  // Разрезаем по текущему времени воспроизведения
                                  // currentTime доступен через глобальный playbackManager или cppTimeline
                                  var splitTime = cppTimeline ? cppTimeline.currentTime : 0
                                  console.log("✂ Split clip", id, "at",
                                              splitTime)
                                  cppTimeline.splitClipAt(splitTime,
                                                          root.trackNumber)
                              }

            onMuteToggled: (id, muted) => {
                               console.log("🔇 Mute clip", id, "→", muted)
                               cppTimeline.setClipMuted(id, muted)
                           }

            onEffectsRequested: id => {
                                    // TODO: открыть панель эффектов
                                    console.log("✨ Effects for clip", id)
                                }
        }
    }
}
