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
    // Cross-track snap: клипы ДРУГОЙ дорожки для магнит-привязки
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

    // ОБНОВИТЬ КЛИПЫ ИЗ C++
    function updateClipsFromCpp() {
        root.clips = cppTimeline.getClipsForTrack(root.trackNumber)
        if (DEBUG_MODE)
            console.log("🔄 Track", root.trackNumber, "клипов:",
                        root.clips.length)
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


    // DROP AREA 1: файлы из проводника Windows / LeftSidebar
    // ВАЖНО: keys: [] (пустой) — принимаем ВСЕ drag-события.
    // Windows Explorer кидает "text/uri-list", а не "video/filepath".
    // Если указать keys: ["video/filepath"], дропы из проводника
    // никогда не принимались — DropArea их просто игнорировала.
    // Клипы ("clip/move") перехватывает clipMoveDropArea (z:6 > z:5),
    // поэтому конфликта нет.

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
                           if (DEBUG_MODE) {
                               console.log(
                                   "⚠️ Drop: не удалось получить путь к файлу")
                           }
                           return
                       }

                       // Фильтр по расширению
                       var ext = filepath.split(".").pop().toLowerCase()
                       var videoExts = ["mp4", "avi", "mov", "mkv", "webm", "flv", "wmv", "m4v", "ts"]
                       if (!videoExts.includes(ext)) {
                           if (DEBUG_MODE)
                           console.log("⚠️ Drop: не видеофайл:", filepath)
                           return
                       }

                       if (DEBUG_MODE) {
                           console.log("🎬 File Drop:", filepath, "at", time,
                                       "sec on track", root.trackNumber)
                       }
                       root.clipDropped(filepath, time)
                   }
    }

    // DROP AREA 2: перенос клипов между дорожками
    // keys["clip/move"] генерирует VideoClip через Drag API

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

                           if (root.clips) {
                               for (var i = 0; i < root.clips.length; i++)
                               if (root.clips[i].id !== clipId)
                               allClips.push(root.clips[i])
                           }
                           if (root.otherTrackClips) {
                               for (var j = 0; j < root.otherTrackClips.length; j++)
                               allClips.push(root.otherTrackClips[j])
                           }

                           // Получаем реальную длину перетаскиваемого клипа
                           var myDuration = 0
                           var info = cppTimeline ? cppTimeline.getClipInfoById(
                                                        clipId) : null
                           if (info && info.duration !== undefined)
                           myDuration = info.duration

                           var snapped = false
                           for (var k = 0; k < allClips.length
                                && !snapped; k++) {
                               var c = allClips[k]
                               var cStart = c.startTime
                               var cEnd = c.startTime + c.duration
                               var myEnd = time + myDuration

                               // Начало нашего клипа → к началу / к концу другого
                               if (Math.abs(time - cStart) < threshold) {
                                   time = cStart
                                   snapped = true
                               } else if (Math.abs(time - cEnd) < threshold) {
                                   time = cEnd
                                   snapped = true
                               } else if (myDuration > 0) {
                                   // Конец нашего → к началу / к концу другого
                                   if (Math.abs(myEnd - cStart) < threshold) {
                                       time = cStart - myDuration
                                       snapped = true
                                   } else if (Math.abs(
                                                  myEnd - cEnd) < threshold) {
                                       time = cEnd - myDuration
                                       snapped = true
                                   }
                               }
                           }

                           if (snapped)
                           if (DEBUG_MODE)
                           console.log("🧲 Snap (cross-track): clip", clipId,
                                       "→", time.toFixed(2), "s")
                       }

                       if (DEBUG_MODE) {
                           console.log("📦 Cross-track drop: clip", clipId,
                                       "→ track", root.trackNumber,
                                       "@ time", time)
                       }
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
    // REPEATER ДЛЯ КЛИПОВ
    Repeater {
        id: clipsRepeater
        model: root.clips

        delegate: VideoClip {
            id: clipItem
            z: 10
            x: modelData.startTime * root.pixelsPerSecond
            y: (root.height - height) / 2
            width: modelData.duration * root.pixelsPerSecond

            clipName: modelData.filename
            clipId: modelData.id
            selected: root.selectedClipId === modelData.id
            // isMuted берём из clipStates (QML-side) — реагирует мгновенно.
            // modelData.isMuted не эмитит dataChanged при setClipMuted в C++.
            // muteVersion читаем явно — Qt 6 не всегда регистрирует dep через var-цепочку внутри функции
            isMuted: {
                var _mv = root.clipStates ? root.clipStates.muteVersion : 0
                return root.clipStates ? root.clipStates.isMuted(
                                             modelData.id) : (modelData.isMuted
                                                              || false)
            }
            // hiddenVersion читаем явно — гарантирует пересчёт при hide/show
            videoHidden: {
                var _hv = root.clipStates ? root.clipStates.hiddenVersion : 0
                return root.clipStates ? root.clipStates.isVideoHidden(
                                             modelData.id) : false
            }
            audioHidden: {
                var _hv = root.clipStates ? root.clipStates.hiddenVersion : 0
                return root.clipStates ? root.clipStates.isAudioHidden(
                                             modelData.id) : false
            }

            // Максимальная ширина = оригинальная длина клипа * текущий масштаб
            // Запрещает растягивать клип длиннее исходного видео
            clipMaxWidth: modelData.duration * root.pixelsPerSecond

            Component.onCompleted: {
                if (DEBUG_MODE) {
                    console.log("🎬 Clip создан:", modelData.filename,
                                "track:", root.trackNumber, "x:", x)
                }
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
            // Обновляем визуальный размер клипа при изменении структуры клипов.
            // Это нужно после trimClip: C++ меняет duration, clipsChanged испускается,
            // Track.qml перестраивает список — но clipItem.width/clipMaxWidth
            // уже созданы с СТАРЫМ modelData.duration и не обновятся сами.
            Connections {
                target: cppTimeline
                function onClipsChanged() {
                    clipItem.x         = modelData.startTime * root.pixelsPerSecond
                    clipItem.width     = modelData.duration  * root.pixelsPerSecond
                    clipItem.clipMaxWidth = modelData.duration * root.pixelsPerSecond
                }
            }

            // СИГНАЛЫ ОТ VideoClip
            onMoved: newX => {
                         var newTime = newX / root.pixelsPerSecond
                         var myDuration = modelData.duration
                         var myEnd = newTime + myDuration

                         // Snap ВКЛ: притягивание + авто-сдвиг при пересечении
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

                             // 1 Сначала пытаемся притянуться к ближайшему краю
                             var snapped = false
                             for (var k = 0; k < allClips.length
                                  && !snapped; k++) {
                                 var c = allClips[k]
                                 var cStart = c.startTime
                                 var cEnd = c.startTime + c.duration

                                 // Начало нашего к началу другого
                                 if (Math.abs(newTime - cStart) < threshold) {
                                     newTime = cStart
                                     snapped = true
                                 } // Начало нашего к концу другого
                                 else if (Math.abs(
                                              newTime - cEnd) < threshold) {
                                     newTime = cEnd
                                     snapped = true
                                 } // Конец нашего к началу другого
                                 else if (Math.abs(
                                              myEnd - cStart) < threshold) {
                                     newTime = cStart - myDuration
                                     snapped = true
                                 } // Конец нашего к концу другого
                                 else if (Math.abs(myEnd - cEnd) < threshold) {
                                     newTime = cEnd - myDuration
                                     snapped = true
                                 }
                             }

                             if (snapped) {
                                 if (DEBUG_MODE) {
                                     console.log("🧲 Snap: clip", modelData.id,
                                                 "→", newTime.toFixed(2), "s")
                                 }
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
                                 if (DEBUG_MODE) {
                                     console.log("⛔ Overlap! Auto-shift clip",
                                                 modelData.id, "→",
                                                 newTime.toFixed(2), "s")
                                 }
                             }
                         }

                         newTime = Math.max(0, newTime)
                         if (DEBUG_MODE) {
                             console.log("📍 Move clip", modelData.id, "→",
                                         newTime.toFixed(2), "sec on track",
                                         root.trackNumber)
                         }
                         root.clipMoved(modelData.id, newTime)
                     }

            // Правый трим: пользователь потянул правый край клипа
            // newPixelWidth — новая визуальная ширина клипа в пикселях.
            // Пересчитываем в duration и вычисляем новый trimEnd = sourceDuration - newDuration - trimStart
            onRightTrimmed: (clipId, newPixelWidth) => {
                if (!cppTimeline) return

                var newDuration = newPixelWidth / root.pixelsPerSecond
                if (newDuration < 0.1) return

                // Получаем текущие данные клипа из C++
                var info = cppTimeline.getClipInfoById(clipId)
                if (!info || info.duration === undefined) return

                var trimStart = info.trimStart !== undefined ? info.trimStart : 0.0
                // sourceDuration = trimStart + текущий duration + trimEnd
                var oldDuration  = info.duration
                var oldTrimEnd   = info.trimEnd   !== undefined ? info.trimEnd : 0.0
                var sourceDuration = trimStart + oldDuration + oldTrimEnd

                // Новый trimEnd = сколько отрезаем с конца
                var newTrimEnd = sourceDuration - trimStart - newDuration
                if (newTrimEnd < 0) newTrimEnd = 0

                if (DEBUG_MODE)
                    console.log("✂ RightTrim clip", clipId,
                                "newDur=", newDuration.toFixed(3),
                                "trimEnd=", newTrimEnd.toFixed(3),
                                "src=", sourceDuration.toFixed(3))

                cppTimeline.trimClip(clipId, trimStart, newTrimEnd)
            }

            onClicked: {
                root.clipSelected(modelData.id)
                root.clipClicked(modelData.id)
            }

            onDeleteRequested: id => {
                                   if (DEBUG_MODE)
                                   console.log("🗑️ Delete clip", id)
                                   if (root.selectedClipId === id)
                                   root.clipSelected(-1)
                                   cppTimeline.removeClip(id)
                                   root.clipDeleted(id)
                               }

            onSplitRequested: id => {
                                  var splitTime = cppTimeline ? cppTimeline.currentTime : 0
                                  if (DEBUG_MODE) {
                                      console.log("✂ Split clip", id, "at",
                                                  splitTime)
                                  }
                                  // splitClip(index, time) — точный разрез по id клипа
                                  if (cppTimeline)
                                  cppTimeline.splitClip(id, splitTime)
                              }

            onMuteToggled: (id, muted) => {
                               if (DEBUG_MODE)
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
