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

                       // ✅ Snap к обеим дорожкам
                       if (root.snapEnabled) {
                           var threshold = 0.5 // 500ms допуск
                           var allClips = []

                           // Собираем ВСЕ клипы кроме текущего
                           if (root.clips) {
                               for (var i = 0; i < root.clips.length; i++) {
                                   if (root.clips[i].id !== clipId)
                                   allClips.push(root.clips[i])
                               }
                           }
                           if (root.otherTrackClips) {
                               for (var j = 0; j < root.otherTrackClips.length; j++) {
                                   allClips.push(root.otherTrackClips[j])
                               }
                           }

                           // Ищем ближайший край — ВСЕ 4 ВАРИАНТА
                           var snapped = false
                           for (var k = 0; k < allClips.length
                                && !snapped; k++) {
                               var c = allClips[k]
                               var cStart = c.startTime
                               var cEnd = c.startTime + c.duration
                               var myEnd = time
                               + (/* duration нужно получить */ 5.0) // TODO: получить duration

                               // Начало нашего клипа → к началу другого
                               if (Math.abs(time - cStart) < threshold) {
                                   time = cStart
                                   snapped = true
                               } // Начало нашего → к концу другого
                               else if (Math.abs(time - cEnd) < threshold) {
                                   time = cEnd
                                   snapped = true
                               } // Конец нашего → к началу другого
                               else if (Math.abs(myEnd - cStart) < threshold) {
                                   time = cStart - (/* duration */ 5.0)
                                   snapped = true
                               } // Конец нашего → к концу другого
                               else if (Math.abs(myEnd - cEnd) < threshold) {
                                   time = cEnd - (/* duration */ 5.0)
                                   snapped = true
                               }
                           }

                           if (snapped) {
                               console.log("🧲 Snap: clip", clipId, "→",
                                           time.toFixed(2), "s")
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
                         var myDuration = modelData.duration
                         var myEnd = newTime + myDuration

                         // *** Snap ВКЛ: притягивание + авто-сдвиг при пересечении ***
                         if (root.snapEnabled) {
                             var threshold = 0.5
                             var allClips = []
                             var hasOverlap = false
                             var overlapEnd = 0

                             // Собираем ВСЕ клипы кроме текущего
                             if (root.clips) {
                                 for (var i = 0; i < root.clips.length; i++) {
                                     if (root.clips[i].id !== modelData.id)
                                     allClips.push(root.clips[i])
                                 }
                             }
                             if (root.otherTrackClips) {
                                 for (var j = 0; j < root.otherTrackClips.length; j++) {
                                     allClips.push(root.otherTrackClips[j])
                                 }
                             }

                             // 1. Сначала пытаемся притянуться к ближайшему краю
                             var snapped = false
                             for (var k = 0; k < allClips.length
                                  && !snapped; k++) {
                                 var c = allClips[k]
                                 var cStart = c.startTime
                                 var cEnd = c.startTime + c.duration

                                 // Начало нашего → к началу другого
                                 if (Math.abs(newTime - cStart) < threshold) {
                                     newTime = cStart
                                     snapped = true
                                 } // Начало нашего → к концу другого
                                 else if (Math.abs(
                                              newTime - cEnd) < threshold) {
                                     newTime = cEnd
                                     snapped = true
                                 } // Конец нашего → к началу другого
                                 else if (Math.abs(
                                              myEnd - cStart) < threshold) {
                                     newTime = cStart - myDuration
                                     snapped = true
                                 } // Конец нашего → к концу другого
                                 else if (Math.abs(myEnd - cEnd) < threshold) {
                                     newTime = cEnd - myDuration
                                     snapped = true
                                 }
                             }

                             if (snapped) {
                                 console.log("🧲 Snap: clip", modelData.id,
                                             "→", newTime.toFixed(2), "s")
                             }

                             // 2. Проверяем пересечения после snap
                             myEnd = newTime + myDuration
                             for (var m = 0; m < allClips.length; m++) {
                                 var c = allClips[m]
                                 var cStart = c.startTime
                                 var cEnd = c.startTime + c.duration

                                 // Проверяем пересечение [newTime, myEnd) vs [cStart, cEnd)
                                 if (!(myEnd <= cStart || newTime >= cEnd)) {
                                     hasOverlap = true
                                     // Запоминаем конец пересекаемого клипа
                                     if (cEnd > overlapEnd)
                                     overlapEnd = cEnd
                                 }
                             }

                             // 3. Если есть пересечение — сдвигаем в конец последнего пересекаемого
                             if (hasOverlap) {
                                 newTime = overlapEnd
                                 console.log("⛔ Overlap! Auto-shift clip",
                                             modelData.id, "→",
                                             newTime.toFixed(2), "s")
                             }
                         }

                         newTime = Math.max(0, newTime)
                         console.log("📍 Move clip", modelData.id, "→",
                                     newTime.toFixed(2), "sec on track",
                                     root.trackNumber)
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
