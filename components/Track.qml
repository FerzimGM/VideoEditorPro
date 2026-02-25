import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.trackBackground
    height: Theme.trackHeight * 2

    property int trackNumber: 1
    property double pixelsPerSecond: 10
    property bool snapEnabled: true
    property var clips: []
    property int selectedClipId: -1

    signal clipClicked(int clipId)
    signal clipSelected(int clipId)
    signal clipDeleted(int clipId)
    signal clipDropped(string filepath, real time)
    signal clipMoved(int clipId, real newTime)

    // ===== ОБНОВИТЬ КЛИПЫ ИЗ C++ =====
    function updateClipsFromCpp() {
        root.clips = cppTimeline.getClipsForTrack(root.trackNumber)
        console.log("🔄 Track", root.trackNumber, "клипов:", root.clips.length)
    }

    Connections {
        target: cppTimeline
        function onClipsChanged() {
            root.updateClipsFromCpp()
        }
    }

    Component.onCompleted: updateClipsFromCpp()

    // Разделитель снизу
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.dividerColor
    }

    // ─────────────────────────────────────────────────────────
    // DROP AREA 1: файлы из файлового менеджера (LeftSidebar)
    // ─────────────────────────────────────────────────────────
    DropArea {
        id: fileDropArea
        anchors.fill: parent
        keys: ["video/filepath"]
        z: 5

        onEntered: dropHighlight.visible = true
        onExited: dropHighlight.visible = false
        onDropped: drop => {
                       dropHighlight.visible = false
                       var filepath = drop.getDataAsString("video/filepath")
                       var time = drop.x / root.pixelsPerSecond
                       console.log("🎬 File Drop:", filepath, "at", time,
                                   "sec on track", root.trackNumber)
                       root.clipDropped(filepath, time)
                   }
    }

    // ─────────────────────────────────────────────────────────
    // DROP AREA 2: перенос клипов между дорожками
    // keys["clip/move"] генерирует VideoClip через Drag API
    // ─────────────────────────────────────────────────────────
    DropArea {
        id: clipMoveDropArea
        anchors.fill: parent
        keys: ["clip/move"]
        z: 6 // Выше fileDropArea

        onEntered: clipMoveHighlight.visible = true
        onExited: clipMoveHighlight.visible = false

        onDropped: drop => {
                       clipMoveHighlight.visible = false
                       var clipId = parseInt(drop.getDataAsString("clip/id"))
                       if (isNaN(clipId) || clipId < 0)
                       return

                       var time = Math.max(0, drop.x / root.pixelsPerSecond)

                       // Snap
                       if (root.snapEnabled && root.clips) {
                           var threshold = 0.5
                           for (var i = 0; i < root.clips.length; i++) {
                               var c = root.clips[i]
                               if (c.id === clipId)
                               continue
                               if (Math.abs(time - c.startTime) < threshold) {
                                   time = c.startTime
                                   break
                               }
                               if (Math.abs(
                                       time - (c.startTime + c.duration)) < threshold) {
                                   time = c.startTime + c.duration
                                   break
                               }
                           }
                       }

                       console.log("📦 Cross-track drop: clip", clipId,
                                   "→ track", root.trackNumber, "@ time", time)
                       if (cppTimeline)
                       cppTimeline.moveClip(clipId, root.trackNumber, time)

                       drop.accept(Qt.MoveAction)
                   }

        // Подсветка при перетаскивании клипа
        Rectangle {
            id: clipMoveHighlight
            anchors.fill: parent
            color: Qt.rgba(0.2, 0.8, 0.4, 0.12)
            border.color: "#43A047"
            border.width: 2
            visible: false
            z: 1
        }
    }

    // Подсветка при дропе файла
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

    // ===== REPEATER ДЛЯ КЛИПОВ =====
    Repeater {
        id: clipsRepeater
        model: root.clips

        delegate: VideoClip {
            id: clipItem

            x: modelData.startTime * root.pixelsPerSecond
            y: (root.height - height) / 2
            width: modelData.duration * root.pixelsPerSecond

            clipName: modelData.filename
            clipId: modelData.id
            selected: root.selectedClipId === modelData.id
            isMuted: modelData.isMuted || false

            // Максимальная ширина = оригинальная длина клипа * текущий масштаб
            // Запрещает растягивать клип длиннее исходного видео
            clipMaxWidth: modelData.duration * root.pixelsPerSecond

            Component.onCompleted: {
                console.log("🎬 Clip создан:", modelData.filename, "track:",
                            root.trackNumber, "x:", x)
            }

            // Обновляем x, width И clipMaxWidth при изменении зума
            Connections {
                target: root
                function onPixelsPerSecondChanged() {
                    clipItem.x = modelData.startTime * root.pixelsPerSecond
                    clipItem.width = modelData.duration * root.pixelsPerSecond
                    clipItem.clipMaxWidth = modelData.duration * root.pixelsPerSecond
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
                                 var curEnd = newTime + modelData.duration
                                 if (Math.abs(
                                         curEnd - otherStart) < snapThreshold) {
                                     newTime = otherStart - modelData.duration
                                     break
                                 }
                             }
                         }

                         newTime = Math.max(0, newTime)
                         console.log("📍 Move clip", modelData.id, "→",
                                     newTime, "sec on track", root.trackNumber)
                         root.clipMoved(modelData.id, newTime)
                     }

            onClicked: {
                root.clipSelected(modelData.id)
                root.clipClicked(modelData.id)
            }

            onDeleteRequested: id => {
                                   console.log("🗑️ Delete clip", id)
                                   if (root.selectedClipId === id)
                                   root.clipSelected(-1)
                                   cppTimeline.removeClip(id)
                                   root.clipDeleted(id)
                               }

            onSplitRequested: id => {
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
                                    console.log("✨ Effects for clip", id)
                                }
        }
    }
}
