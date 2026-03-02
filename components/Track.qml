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
    property var clipStates: null
    // *** Cross-track snap: клипы ДРУГОЙ дорожки для магнит-привязки ***
    // Если магнит включён, перетаскиваемый клип притягивается к концам
    // клипов как своей дорожки, так и дорожки-партнёра.
    property var otherTrackClips: []

    signal clipClicked(int clipId)
    signal clipSelected(int clipId)
    signal clipDeleted(int clipId)
    signal clipDropped(string filepath, real time)
    signal clipMoved(int clipId, real newTime)
    signal effectsRequested(int clipId)
    // Пробрасываем запрос контекстного меню наверх в Timeline.qml
    signal contextMenuRequested(int clipId, bool isVideo, real globalX, real globalY)

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
    // DROP AREA 1: файлы из проводника Windows / LeftSidebar
    // ВАЖНО: keys: [] (пустой) — принимаем ВСЕ drag-события.
    // Windows Explorer кидает "text/uri-list", а не "video/filepath".
    // Если указать keys: ["video/filepath"], дропы из проводника
    // никогда не принимались — DropArea их просто игнорировала.
    // Клипы ("clip/move") перехватывает clipMoveDropArea (z:6 > z:5),
    // поэтому конфликта нет.
    // ─────────────────────────────────────────────────────────
    DropArea {
        id: fileDropArea
        anchors.fill: parent
        keys: [] // пустой = принимать любые drag-данные
        z: 5

        onEntered: function (drag) {
            // Подсветка только для файлов, не для клипов
            if (!drag.keys.includes("clip/move")) {
                dropHighlight.visible = true
            }
        }
        onExited: dropHighlight.visible = false

        onDropped: drop => {
                       dropHighlight.visible = false

                       // Клипы обрабатывает clipMoveDropArea — не трогаем
                       if (drop.keys.includes("clip/move")) {
                           drop.accepted = false
                           return
                       }

                       var filepath = ""
                       var time = drop.x / root.pixelsPerSecond

                       // Вариант 1: drag из LeftSidebar (кастомный ключ)
                       if (drop.keys.includes("video/filepath")) {
                           filepath = drop.getDataAsString("video/filepath")
                       } // Вариант 2: drag из проводника Windows (text/uri-list)
                       else if (drop.hasUrls && drop.urls.length > 0) {
                           var url = drop.urls[0].toString()
                           filepath = url.replace(/^file:\/\/\//, "")
                           // Windows: /C:/path → C:/path
                           if (filepath.match(/^\/[A-Za-z]:\//)) {
                               filepath = filepath.substring(1)
                           }
                       } // Вариант 3: текст с путём
                       else if (drop.hasText) {
                           filepath = drop.text.trim().replace(/^file:\/\/\//,
                                                               "")
                       }

                       if (filepath === "") {
                           console.log(
                               "⚠️ Drop: не удалось получить путь к файлу")
                           return
                       }

                       // Фильтр по расширению
                       var ext = filepath.split(".").pop().toLowerCase()
                       var videoExts = ["mp4", "avi", "mov", "mkv", "webm", "flv", "wmv", "m4v", "ts"]
                       if (!videoExts.includes(ext)) {
                           console.log("⚠️ Drop: не видеофайл:", filepath)
                           return
                       }

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
                           var allSnap = (root.clips || []).concat(root.otherTrackClips || [])
                           for (var i = 0; i < allSnap.length; i++) {
                               var c = allSnap[i]
                               if (c.id === clipId)
                               continue
                               var cEnd = c.startTime + c.duration
                               if (Math.abs(time - c.startTime) < threshold) {
                                   time = c.startTime
                                   break
                               }
                               if (Math.abs(time - cEnd) < threshold) {
                                   time = cEnd
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
            z: 10 // Выше DropArea (fileDropArea z:5, clipMoveDropArea z:6)

            // Без z:10 DropArea перехватывает ПКМ — контекстное меню не открывается
            x: modelData.startTime * root.pixelsPerSecond
            y: (root.height - height) / 2
            width: modelData.duration * root.pixelsPerSecond

            clipName: modelData.filename
            clipId: modelData.id
            selected: root.selectedClipId === modelData.id
            // isMuted берём из clipStates (QML-side) — реагирует мгновенно.
            // modelData.isMuted не эмитит dataChanged при setClipMuted в C++.
            isMuted: root.clipStates ? root.clipStates.isMuted(
                                           modelData.id) : (modelData.isMuted
                                                            || false)
            videoHidden: root.clipStates ? root.clipStates.isVideoHidden(
                                               modelData.id) : false
            audioHidden: root.clipStates ? root.clipStates.isAudioHidden(
                                               modelData.id) : false

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

                         // *** Snap к краям клипов: своя дорожка + дорожка-партнёр ***
                         // При включённом магните притягиваемся к startTime и endTime
                         // всех клипов обеих дорожек (кроме самого перетаскиваемого).
                         if (root.snapEnabled) {
                             var snapThreshold = 0.5
                             // Объединяем клипы своей и чужой дорожки
                             var allClips = (root.clips || []).concat(root.otherTrackClips || [])
                             var snapped = false
                             for (var i = 0; i < allClips.length && !snapped; i++) {
                                 var c = allClips[i]
                                 if (c.id === modelData.id)
                                     continue

                                 var otherStart = c.startTime
                                 var otherEnd = c.startTime + c.duration

                                 // Начало нашего клипа → к началу другого
                                 if (Math.abs(newTime - otherStart) < snapThreshold) {
                                     newTime = otherStart
                                     snapped = true
                                 // Начало нашего клипа → к концу другого
                                 } else if (Math.abs(newTime - otherEnd) < snapThreshold) {
                                     newTime = otherEnd
                                     snapped = true
                                 // Конец нашего клипа → к началу другого
                                 } else {
                                     var curEnd = newTime + modelData.duration
                                     if (Math.abs(curEnd - otherStart) < snapThreshold) {
                                         newTime = otherStart - modelData.duration
                                         snapped = true
                                     // Конец нашего клипа → к концу другого
                                     } else if (Math.abs(curEnd - otherEnd) < snapThreshold) {
                                         newTime = otherEnd - modelData.duration
                                         snapped = true
                                     }
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
                                  // splitClip(index, time) — точный разрез по id клипа
                                  if (cppTimeline)
                                  cppTimeline.splitClip(id, splitTime)
                              }

            onMuteToggled: (id, muted) => {
                               console.log("🔇 Mute clip", id, "→", muted)
                               cppTimeline.setClipMuted(id, muted)
                           }

            onEffectsRequested: id => {
                                    root.effectsRequested(id)
                                }

            onContextMenuRequested: (id, isVideo, gx, gy) => {
                                        root.contextMenuRequested(id, isVideo,
                                                                  gx, gy)
                                    }
        }
    }
}
