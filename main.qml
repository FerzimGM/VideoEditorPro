import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"
import "theme.js" as Theme

QtObject {
    id: appRoot

    // ОКНО 1: SPLASH SCREEN
    property var splashWindow: ApplicationWindow {
        id: splashWin
        visible: true
        width: 800
        height: 435
        color: "transparent"
        flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        Component.onCompleted: {
            x = (Screen.width - width) / 2
            y = (Screen.height - height) / 2
        }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.95
            radius: Theme.borderRadius
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.backgroundColor
            radius: Theme.borderRadius
            border.color: Theme.rubyPrimary
            border.width: 2
            SplashScreen {
                anchors.fill: parent
                onLoaded: {
                    splashWin.close()
                    mainWindow.visible = true
                }
            }
        }
    }

    // ГЛАВНОЕ ОКНО
    property var mainWindow: ApplicationWindow {
        id: root
        visible: false
        width: 1600
        height: 900
        minimumWidth: 1280
        minimumHeight: 720
        title: "VideoEditor Pro"
        color: Theme.backgroundColor
        flags: Qt.Window | Qt.FramelessWindowHint

        // cutKeyPressed объявлен здесь, в Window root
        // Shortcuts тоже в Window root - доступ без проблем
        property bool cutKeyPressed: false

        // ПАРАМЕТРЫ ЭКСПОРТА (из ExportPanel в LeftSidebar)
        // Сохраняются когда пользователь нажимает "Сохранить видео"
        property string _exportResolution: "1920×1080"
        property string _exportFormat: "MP4"

        // Resize handles
        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 5
            cursorShape: Qt.SizeHorCursor
            z: 1000
            property real sx: 0
            property real sw: 0
            onPressed: mouse => {
                           sx = mouseX + root.x + root.width - 5
                           sw = root.width
                       }
            onMouseXChanged: {
                if (pressed)
                    root.width = Math.max(
                                root.minimumWidth,
                                sw + (mouseX + root.x + root.width - 5 - sx))
            }
        }
        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 5
            cursorShape: Qt.SizeVerCursor
            z: 1000
            property real sy: 0
            property real sh: 0
            onPressed: mouse => {
                           sy = mouseY + root.y + root.height - 5
                           sh = root.height
                       }
            onMouseYChanged: {
                if (pressed)
                    root.height = Math.max(
                                root.minimumHeight,
                                sh + (mouseY + root.y + root.height - 5 - sy))
            }
        }
        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 10
            height: 10
            cursorShape: Qt.SizeFDiagCursor
            z: 1001
            property real sx: 0
            property real sy: 0
            property real sw: 0
            property real sh: 0
            onPressed: mouse => {
                           sx = mouseX + root.x + root.width - 10
                           sy = mouseY + root.y + root.height - 10
                           sw = root.width
                           sh = root.height
                       }
            onPositionChanged: mouse => {
                                   if (pressed) {
                                       root.width = Math.max(
                                           root.minimumWidth,
                                           sw + (mouse.x + root.x + root.width - 10 - sx))
                                       root.height = Math.max(
                                           root.minimumHeight,
                                           sh + (mouse.y + root.y + root.height - 10 - sy))
                                   }
                               }
        }

        // Менеджеры
        QtObject {
            id: projectManager
            property string currentProjectPath: ""
            property bool isProjectModified: false
        }

        ClipManager {
            id: clipManager
        }
        ClipEffectsDialog {
            id: clipEffectsDialog
            parentWindow: root
            visible: false
        }

        // КОНТЕКСТНЫЕ МЕНЮ
        // В дочерних компонентах (.qml файлах) Overlay.overlay возвращает null в Qt 6.
        // Хранит видимость видео/аудио полос каждого клипа
        QtObject {
            id: clipStates
            property var _hidden: ({})
            property var _muted: ({})
            property int muteVersion: 0
            property int hiddenVersion: 0

            function isMuted(clipId) {
                return _muted[clipId] === true
            }
            function setMuted(clipId, val) {
                var m = Object.assign({}, _muted)
                m[clipId] = val
                _muted = m
                muteVersion++
                if (cppTimeline)
                    cppTimeline.setClipMuted(clipId, val)
            }

            function setVideoHidden(clipId, val) {
                var h = Object.assign({}, _hidden)
                h[clipId + "_v"] = val
                _hidden = h
                hiddenVersion++
                if (cppTimeline)
                    cppTimeline.setClipVideoHidden(clipId, val)
            }
            function setAudioHidden(clipId, val) {
                var h = Object.assign({}, _hidden)
                h[clipId + "_a"] = val
                _hidden = h
                hiddenVersion++
                if (cppTimeline)
                    cppTimeline.setClipAudioHidden(clipId, val)
            }
            function isVideoHidden(clipId) {
                return _hidden[clipId + "_v"] === true
            }
            function isAudioHidden(clipId) {
                return _hidden[clipId + "_a"] === true
            }
        }

        //  КЭШ КЛИПОВ — обновляется только при clipsChanged
        // Устраняет спам getClipsForTrack: раньше hideVideo/hideAudio вызывали
        // getClipsForTrack при каждом изменении currentTime (25+ раз в секунду).
        QtObject {
            id: clipsCache
            property var track1: []
            property var track2: []
        }
        Connections {
            target: cppTimeline
            function onClipsChanged() {
                // Обновляем кэш клипов для биндингов VideoPlayer (hideVideo, hideAudio1/2)
                clipsCache.track1 = cppTimeline.getClipsForTrack(1)
                clipsCache.track2 = cppTimeline.getClipsForTrack(2)
            }
            // currentTime обновляется через onTimePositionChanged (из playbackTimeUpdated)
            // Здесь НЕ обновляем — иначе двойной цикл обновлений
            function onRenderProgress(percent) {
                exportDialog.renderProgress = percent
            }
            function onRenderFinished(success) {
                exportDialog.isRendering = false
                if (success) {
                    if (DEBUG_MODE)
                        console.log("✅ Экспорт завершён!")
                    exportDialog.close()
                } else {
                    if (DEBUG_MODE)
                        console.log("❌ Ошибка экспорта")
                }
            }
        }

        QtObject {
            id: menuContext
            property int clipId: -1
            property int track: 1
            property string clipName: ""
            property bool isMuted: false
            property bool videoHidden: false
        }

        Menu {
            id: videoContextMenu
            parent: Overlay.overlay
            width: 240

            background: Rectangle {
                color: "#1E1E2E"
                radius: 6
                border.color: "#DC143C"
                border.width: 1
            }
            MenuItem {
                enabled: false
                contentItem: Text {
                    text: "📹  " + menuContext.clipName
                    color: "#999"
                    font.bold: true
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: "transparent"
                }
            }
            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: "#444"
                }
            }
            MenuItem {
                text: "✂  Разрезать по playhead"
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                onTriggered: if (cppTimeline)
                                 cppTimeline.splitClipAt(
                                             cppTimeline.currentTime,
                                             menuContext.track)
            }
            MenuItem {
                text: "✨  Эффекты клипа..."
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                onTriggered: clipEffectsDialog.openForClip(menuContext.clipId)
            }
            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: "#444"
                }
            }
            MenuItem {
                implicitHeight: 34
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                contentItem: Text {
                    text: clipStates.isVideoHidden(
                              menuContext.clipId) ? "👁  Показать видео" : "🙈  Скрыть видео"
                    color: "white"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                onTriggered: {
                    var nowHidden = clipStates.isVideoHidden(menuContext.clipId)
                    clipStates.setVideoHidden(menuContext.clipId, !nowHidden)
                    if (DEBUG_MODE)
                        console.log("🙈 Видео скрыто:", !nowHidden)
                }
            }
            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: "#444"
                }
            }
            MenuItem {
                implicitHeight: 34
                background: Rectangle {
                    color: parent.highlighted ? Qt.rgba(0.87, 0.13, 0.23,
                                                        0.15) : "transparent"
                }
                contentItem: Text {
                    text: "🗑  Удалить клип"
                    color: "#EF5350"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                onTriggered: if (cppTimeline)
                                 cppTimeline.removeClip(menuContext.clipId)
            }
        }

        Menu {
            id: audioContextMenu
            parent: Overlay.overlay
            width: 240

            background: Rectangle {
                color: "#1E1E2E"
                radius: 6
                border.color: "#43A047"
                border.width: 1
            }
            MenuItem {
                enabled: false
                contentItem: Text {
                    text: "🎵  " + menuContext.clipName
                    color: "#999"
                    font.bold: true
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: "transparent"
                }
            }
            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: "#444"
                }
            }
            MenuItem {
                text: menuContext.isMuted ? "🔊  Включить звук" : "🔇  Выключить звук"
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                onTriggered: {
                    var newMuted = !menuContext.isMuted
                    menuContext.isMuted = newMuted
                    clipStates.setMuted(menuContext.clipId, newMuted)
                    // setClipMuted уже вызван внутри clipStates.setMuted — не дублируем
                }
            }
            MenuItem {
                text: "✂  Разрезать по playhead"
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                onTriggered: if (cppTimeline)
                                 cppTimeline.splitClipAt(
                                             cppTimeline.currentTime,
                                             menuContext.track)
            }
            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: "#444"
                }
            }
            MenuItem {
                text: "🗑  Удалить клип"
                contentItem: Text {
                    text: parent.text
                    color: "#EF5350"
                    font.pixelSize: 12
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.highlighted ? "#2a2a3e" : "transparent"
                }
                onTriggered: if (cppTimeline)
                                 cppTimeline.removeClip(menuContext.clipId)
            }
        }

        QtObject {
            id: playbackManager
            property real currentTime: 0
            property real duration: cppTimeline ? Math.max(
                                                      60,
                                                      cppTimeline.totalDuration) : 60
            property bool isPlaying: false
            property real playbackSpeed: 1.0
            property real volume: 1.0
            property int zoomLevel: 100
            property bool snapEnabled: true
        }

        // МЕНЕДЖЕР ВЫДЕЛЕНИЯ
        // selectionManager доступен из VideoClip.qml и Track.qml по id
        QtObject {
            id: selectionManager
            property int selectedClipId: -1
            function clearSelection() {
                selectedClipId = -1
            }
        }

        Connections {
            target: cppTimeline
            function onTotalDurationChanged() {
                if (DEBUG_MODE)
                    console.log("⏱️ Duration:", cppTimeline.totalDuration)
            }
            // Сбрасываем выделение ТОЛЬКО при удалении клипа,
            // чтобы при перемещении выделение не слетало
            function onClipRemoved(index) {
                if (selectionManager.selectedClipId === index
                        || selectionManager.selectedClipId >= cppTimeline.clipCount) {
                    selectionManager.clearSelection()
                }
            }
        }

        //  ДИАЛОГИ
        FileDialog {
            id: openVideoDialog
            title: "Открыть видео"
            nameFilters: ["Video files (*.mp4 *.avi *.mov *.mkv)", "All files (*)"]
            onAccepted: {
                var filepath = selectedFile.toString()
                filepath = filepath.replace(/^file:\/\/\//, "")
                if (filepath.match(/^\/[A-Za-z]:\//))
                    filepath = filepath.substring(1)

                // добавляем в конец последнего клипа, не на текущее время
                // Если видеофайлы уже есть — ставим новый после них
                // Если нет — ставим на 0
                var startTime = 0
                if (cppTimeline) {
                    var trackEnd = cppTimeline.getTrackEndTime(1)
                    // Если playhead стоит ДО конца клипов — добавляем в конец клипов
                    // Если playhead стоит ПОСЛЕ — добавляем на playhead (ручное позиционирование)
                    startTime = Math.max(
                                trackEnd,
                                playbackManager.currentTime === 0 ? 0 : playbackManager.currentTime)
                    // Если нет клипов — добавляем на 0
                    if (trackEnd === 0)
                        startTime = 0
                    else
                        startTime = trackEnd // всегда в конец для простоты
                }

                if (DEBUG_MODE) {
                    console.log("Добавляю видео:", filepath, "startTime:",
                                startTime)
                }
                clipManager.addClip(filepath, 1, startTime)
            }
        }

        FileDialog {
            id: openProjectDialog
            title: "Открыть проект"
            nameFilters: ["Project files (*.vep)", "All files (*)"]
            onAccepted: {
                var filepath = selectedFile.toString()
                filepath = filepath.replace(/^file:\/\/\//, "")
                if (filepath.match(/^\/[A-Za-z]:\//))
                    filepath = filepath.substring(1)
                projectManager.currentProjectPath = filepath
                cppTimeline.loadProject(filepath)
            }
        }

        FileDialog {
            id: saveProjectDialog
            fileMode: FileDialog.SaveFile
            title: "Сохранить проект"
            nameFilters: ["Project files (*.vep)", "All files (*)"]
            defaultSuffix: "vep"
            onAccepted: {
                var path = selectedFile.toString()
                path = path.replace(/^file:\/\/\//, "")
                if (path.match(/^\/[A-Za-z]:\//))
                    path = path.substring(1)
                projectManager.currentProjectPath = path
                projectManager.isProjectModified = false
                cppTimeline.saveProject(path)
            }
        }

        FileDialog {
            id: exportFileDialog
            fileMode: FileDialog.SaveFile
            title: "Экспорт видео"
            nameFilters: ["MP4 (*.mp4)", "AVI (*.avi)", "MOV (*.mov)", "MKV (*.mkv)", "WebM (*.webm)"]
            defaultSuffix: "mp4"
            onAccepted: {
                var filepath = selectedFile.toString()
                filepath = filepath.replace(/^file:\/\/\//, "")
                if (filepath.match(/^\/[A-Za-z]:\//))
                    filepath = filepath.substring(1)

                exportDialog.isRendering = true
                exportDialog.renderProgress = 0
                exportDialog.open()

                // Разрешение из ExportPanel (сохранено в root)
                var resText = root._exportResolution || "1920×1080"
                var w = 1920, h = 1080
                if (resText.indexOf("1280") >= 0) {
                    w = 1280
                    h = 720
                } else if (resText.indexOf("3840") >= 0) {
                    w = 3840
                    h = 2160
                } else if (resText.indexOf("2560") >= 0) {
                    w = 2560
                    h = 1440
                }

                var fmt = root._exportFormat || "MP4"

                // Синхронизируем clipStates -> C++ и запускаем рендер.
                // ВАЖНО: используем UID клипов из clipsCache, НЕ sequential-индексы.
                // clipStates._hidden хранит по UID ("5_v": true).
                // isVideoHidden(0) никогда не найдёт скрытый клип с UID=5.
                var hiddenMap = {}
                var mutedMap = {}
                var allClips = clipsCache.track1.concat(clipsCache.track2)
                for (var ci = 0; ci < allClips.length; ci++) {
                    var clipId = allClips[ci].id
                    hiddenMap[clipId + "_v"] = clipStates.isVideoHidden(clipId)
                    hiddenMap[clipId + "_a"] = clipStates.isAudioHidden(clipId)
                    mutedMap[clipId] = clipStates.isMuted(clipId)
                }
                cppTimeline.syncClipStatesForRender(hiddenMap, mutedMap)
                cppTimeline.renderToFile(filepath, w, h, fmt)
            }
        }

        // Окно рендера — frameless, поверх всех, не скрывается
        Window {
            id: exportDialog
            title: "Экспорт видео"
            width: 400
            height: 420
            color: "transparent"
            flags: Qt.Window | Qt.FramelessWindowHint
            modality: Qt.NonModal

            property int renderProgress: 0
            property bool isRendering: false

            function open() {
                if (root) {
                    x = root.x + (root.width - width) / 2
                    y = root.y + (root.height - height) / 2
                }
                visible = true
                raise()
                requestActivate()
            }
            function close() {
                visible = false
            }

            // Запрещаем закрытие во время рендера через сигнал
            Connections {
                target: exportDialog
                function onClosing(close) {
                    if (exportDialog.isRendering)
                        close.accepted = false
                }
            }

            // Фон
            Rectangle {
                anchors.fill: parent
                color: Theme.backgroundColor
                radius: Theme.borderRadius + 2
                border.color: Theme.rubyPrimary
                border.width: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Заголовок (с drag)
                    Rectangle {
                        Layout.fillWidth: true
                        height: 46
                        color: Theme.backgroundDark
                        radius: Theme.borderRadius + 2
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: parent.radius
                            color: parent.color
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            spacing: 10
                            Text {
                                text: "🎬"
                                font.pixelSize: 18
                            }
                            Text {
                                text: exportDialog.isRendering ? "Рендеринг..." : "Экспорт видео"
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            // Drag-зона
                            MouseArea {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                cursorShape: Qt.SizeAllCursor
                                property real sx: 0
                                property real sy: 0
                                onPressed: function (e) {
                                    sx = e.x
                                    sy = e.y
                                }
                                onPositionChanged: function (e) {
                                    if (pressed) {
                                        exportDialog.x += e.x - sx
                                        exportDialog.y += e.y - sy
                                    }
                                }
                            }
                            // Крестик — только когда НЕ рендерим
                            Rectangle {
                                visible: !exportDialog.isRendering
                                width: 28
                                height: 28
                                radius: 14
                                color: expCloseMa.containsMouse ? Theme.rubyPrimary : "transparent"
                                border.color: Theme.rubyPrimary
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 100
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                MouseArea {
                                    id: expCloseMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: exportDialog.close()
                                }
                            }
                        }
                    }

                    // Настройки экспорта (только когда не рендерим)
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 20
                        spacing: 12
                        visible: !exportDialog.isRendering

                        Text {
                            text: "Разрешение"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                        }
                        ComboBox {
                            id: resolutionCombo
                            Layout.fillWidth: true
                            model: ["1920×1080 (Full HD)", "1280×720 (HD)", "3840×2160 (4K)", "2560×1440 (2K)"]
                            contentItem: Text {
                                text: parent.displayText
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                            }
                            background: Rectangle {
                                color: Theme.backgroundDark
                                border.color: Theme.borderLight
                                border.width: 1
                                radius: Theme.borderRadius
                            }
                        }

                        Text {
                            text: "Формат"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                        }
                        ComboBox {
                            id: exportFormatCombo
                            Layout.fillWidth: true
                            model: ["MP4", "AVI", "MOV", "MKV", "WebM"]
                            contentItem: Text {
                                text: parent.displayText
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                            }
                            background: Rectangle {
                                color: Theme.backgroundDark
                                border.color: Theme.borderLight
                                border.width: 1
                                radius: Theme.borderRadius
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            text: "▶  Экспортировать"
                            onClicked: {
                                var format = exportFormatCombo.currentText
                                root._exportResolution = resolutionCombo.currentText
                                root._exportFormat = format
                                var extMap = {
                                    "MP4": "mp4",
                                    "AVI": "avi",
                                    "MOV": "mov",
                                    "MKV": "mkv",
                                    "WebM": "webm"
                                }
                                var ext = extMap[format] || "mp4"
                                exportFileDialog.defaultSuffix = ext
                                var filterLabel = {
                                    "mp4": "MP4 (*.mp4)",
                                    "avi": "AVI (*.avi)",
                                    "mov": "MOV (*.mov)",
                                    "mkv": "MKV (*.mkv)",
                                    "webm": "WebM (*.webm)"
                                }
                                exportFileDialog.nameFilters = [filterLabel[ext]
                                                                || "MP4 (*.mp4)"]
                                exportFileDialog.open()
                            }
                            contentItem: Text {
                                text: parent.text
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary)
                                radius: Theme.borderRadius
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 100
                                    }
                                }
                            }
                        }
                    }

                    // Круговой прогресс
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: exportDialog.isRendering

                        // Круг прогресса
                        Canvas {
                            id: progressRing
                            anchors.centerIn: parent
                            width: 160
                            height: 160

                            property real progress: exportDialog.renderProgress / 100.0

                            onProgressChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = 68
                                var lw = 10

                                // Фон кольца
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                ctx.strokeStyle = "#1a1a2e"
                                ctx.lineWidth = lw
                                ctx.stroke()

                                // Прогресс кольца
                                if (progress > 0) {
                                    var grad = ctx.createLinearGradient(
                                                cx - r, cy, cx + r, cy)
                                    grad.addColorStop(0, "#EF5350")
                                    grad.addColorStop(1, "#FF8A65")
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r, -Math.PI / 2,
                                            -Math.PI / 2 + progress * Math.PI * 2)
                                    ctx.strokeStyle = grad
                                    ctx.lineWidth = lw
                                    ctx.lineCap = "round"
                                    ctx.stroke()
                                }

                                // Внутренний блик
                                ctx.beginPath()
                                ctx.arc(cx, cy, r - lw / 2 - 4, 0, Math.PI * 2)
                                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.04)
                                ctx.lineWidth = 1
                                ctx.stroke()
                            }

                            // Процент в центре
                            Column {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: exportDialog.renderProgress + "%"
                                    color: Theme.rubyLight
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 28
                                    font.bold: true
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "рендер"
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    font.letterSpacing: 1
                                }
                            }
                        }
                    }

                    // Кнопка отмены
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.bottomMargin: 20
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 140
                        visible: exportDialog.isRendering
                        text: "✕  Отмена"
                        onClicked: {
                            cppTimeline.cancelRender()
                            exportDialog.isRendering = false
                            exportDialog.close()
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#EF5350"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.down ? Qt.rgba(
                                                     .94, .33, .31,
                                                     .3) : (parent.hovered ? Qt.rgba(.94, .33, .31, .15) : "transparent")
                            border.color: "#EF5350"
                            border.width: 1
                            radius: Theme.borderRadius
                        }
                    }
                }
            }
        }

        // Верхняя панель
        TopMenuBar {
            id: menuBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.panelHeight
            z: 100
            targetWindow: root
            onOpenVideo: openVideoDialog.open()
            onOpenProject: openProjectDialog.open()
            onSaveProject: saveProjectDialog.open()
            onExportVideo: exportDialog.open()
            onMinimize: root.showMinimized()
            onMaximize: root.visibility === Window.Maximized ? root.showNormal(
                                                                   ) : root.showMaximized()
            onClose: Qt.quit()
        }

        // ГЛАВНЫЙ LAYOUT
        ColumnLayout {
            anchors.top: menuBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 0

            // Рабочая область
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                LeftSidebar {
                    id: leftSidebar
                    Layout.preferredWidth: Theme.sidebarWidth
                    Layout.fillHeight: true
                    videoPlayer: videoPlayer
                    selectedClipId: selectionManager.selectedClipId

                    // Кнопка "СОХРАНИТЬ ВИДЕО" в ExportPanel
                    onExportRequested: (resolution, format) => {
                                           root._exportResolution = resolution
                                           root._exportFormat = format
                                           var extMap = {
                                               "MP4": "mp4",
                                               "AVI": "avi",
                                               "MOV": "mov",
                                               "MKV": "mkv",
                                               "WebM": "webm"
                                           }
                                           var ext = extMap[format] || "mp4"
                                           exportFileDialog.defaultSuffix = ext
                                           // Переставляем фильтры — выбранный формат первым
                                           var allF = ["MP4 (*.mp4)", "AVI (*.avi)", "MOV (*.mov)", "MKV (*.mkv)", "WebM (*.webm)"]
                                           var allE = ["mp4", "avi", "mov", "mkv", "webm"]
                                           var fi = allE.indexOf(ext)
                                           if (fi >= 0) {
                                               var reordered = [allF[fi]]
                                               for (var i = 0; i < allF.length; i++)
                                               if (i !== fi)
                                               reordered.push(allF[i])
                                               exportFileDialog.nameFilters = reordered
                                           }
                                           exportFileDialog.open()
                                       }
                }

                // Центральная колонка
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // Верхний ряд: ModeSwitcher + VideoPlayer
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spacing

                        ModeSwitcher {
                            id: modeSwitcher
                            Layout.preferredWidth: 180
                            Layout.fillHeight: true
                            Layout.margins: Theme.spacing
                            onModeChanged: mode => {
                                               leftSidebar.currentMode = mode
                                           }
                        }

                        VideoPlayer {
                            id: videoPlayer
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.margins: Theme.spacing

                            currentTime: playbackManager.currentTime
                            duration: playbackManager.duration
                            isPlaying: playbackManager.isPlaying
                            //  Передаём скорость - QMediaPlayer.playbackRate
                            playbackSpeed: playbackManager.playbackSpeed
                            volume: playbackManager.volume
                            // hideVideo=true только если track1 скрыт И track2 пустой - чёрный экран
                            hideVideo: {
                                var _hv = clipStates._hidden
                                if (!cppTimeline || !clipStates)
                                    return false
                                var t = playbackManager.currentTime
                                var c1 = clipsCache.track1
                                var track1HiddenHere = false
                                for (var i = 0; i < c1.length; i++) {
                                    if (t >= c1[i].startTime
                                            && t < c1[i].startTime + c1[i].duration) {
                                        track1HiddenHere = clipStates.isVideoHidden(
                                                    c1[i].id)
                                        break
                                    }
                                }
                                if (!track1HiddenHere)
                                    return false
                                // track1 скрыт — есть ли что-то на track2?
                                var c2 = clipsCache.track2
                                for (var j = 0; j < c2.length; j++) {
                                    if (t >= c2[j].startTime
                                            && t < c2[j].startTime + c2[j].duration)
                                        return false // track2 есть - показываем его
                                }
                                return true // оба пусты/скрыты - чёрный экран
                            }
                            // hideTrack1Video: скрыть только videoOutput1 (track1), track2 остаётся
                            hideTrack1Video: {
                                var _hv2 = clipStates._hidden
                                if (!cppTimeline || !clipStates)
                                    return false
                                var t2 = playbackManager.currentTime
                                var clips1 = clipsCache.track1
                                for (var ii = 0; ii < clips1.length; ii++) {
                                    if (t2 >= clips1[ii].startTime
                                            && t2 < clips1[ii].startTime + clips1[ii].duration)
                                        return clipStates.isVideoHidden(
                                                    clips1[ii].id)
                                }
                                return false
                            }
                            // Per-track audio muting: каждый трек глушится независимо
                            hideAudio1: {
                                var _mv1 = clipStates.muteVersion
                                var _ha1 = clipStates._hidden
                                if (!cppTimeline || !clipStates)
                                    return false
                                var t1 = playbackManager.currentTime
                                var tc1 = clipsCache.track1
                                for (var i1 = 0; i1 < tc1.length; i1++) {
                                    if (t1 >= tc1[i1].startTime
                                            && t1 < tc1[i1].startTime + tc1[i1].duration)
                                        return clipStates.isAudioHidden(
                                                    tc1[i1].id)
                                                || clipStates.isMuted(
                                                    tc1[i1].id)
                                }
                                return false
                            }
                            hideAudio2: {
                                var _mv2 = clipStates.muteVersion
                                var _ha2 = clipStates._hidden
                                if (!cppTimeline || !clipStates)
                                    return false
                                var t2 = playbackManager.currentTime
                                var tc2 = clipsCache.track2
                                for (var i2 = 0; i2 < tc2.length; i2++) {
                                    if (t2 >= tc2[i2].startTime
                                            && t2 < tc2[i2].startTime + tc2[i2].duration)
                                        return clipStates.isAudioHidden(
                                                    tc2[i2].id)
                                                || clipStates.isMuted(
                                                    tc2[i2].id)
                                }
                                return false
                            }

                            // Обновляем playhead — только UI, не пишем в C++ во время воспроизведения
                            onTimePositionChanged: time => {
                                                       if (Math.abs(
                                                               playbackManager.currentTime
                                                               - time) > 0.05) {
                                                           playbackManager.currentTime = time
                                                           // НЕ пишем cppTimeline.currentTime во время воспроизведения:
                                                           // это вызывает setCurrentTime - seekTo - сброс кэша - Cache miss
                                                           if (!playbackManager.isPlaying)
                                                           cppTimeline.currentTime = time
                                                       }
                                                   }
                            // VideoPlayer не может писать isPlaying=false напрямую
                            // (сломает QML binding). Вместо этого — сигнал.
                            onPlaybackStopped: {
                                playbackManager.isPlaying = false
                                playbackManager.currentTime = 0
                                cppTimeline.currentTime = 0
                            }
                        }
                    }

                    // PlaybackControls — НИЖЕ видеоплеера
                    PlaybackControls {
                        id: playbackControls
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.toolbarHeight

                        isPlaying: playbackManager.isPlaying
                        currentTime: playbackManager.currentTime
                        duration: playbackManager.duration
                        playbackSpeed: playbackManager.playbackSpeed
                        snapEnabled: playbackManager.snapEnabled
                        volume: playbackManager.volume

                        onPlayPauseClicked: playbackManager.isPlaying = !playbackManager.isPlaying

                        onStopClicked: {
                            if (playbackManager.isPlaying) {
                                cppTimeline.stopPlayback()
                                playbackManager.isPlaying = false
                            }
                            playbackManager.currentTime = 0
                            cppTimeline.currentTime = 0
                        }

                        onSeek: time => {
                                    var t = Math.max(
                                        0, Math.min(time,
                                                    playbackManager.duration))
                                    playbackManager.currentTime = t
                                    if (playbackManager.isPlaying) {
                                        // Перезапускаем с новой позиции
                                        cppTimeline.startPlayback(
                                            t, playbackManager.playbackSpeed)
                                    } else {
                                        cppTimeline.currentTime = t
                                    }
                                }

                        onSpeedChanged: speed => {
                                            playbackManager.playbackSpeed = speed
                                            if (playbackManager.isPlaying)
                                            cppTimeline.startPlayback(
                                                playbackManager.currentTime,
                                                speed)
                                        }

                        onVolumeChanged: {
                            playbackManager.volume = volume
                            cppTimeline.setPlaybackVolume(volume)
                        }
                        onSnapToggled: {
                            playbackManager.snapEnabled = !playbackManager.snapEnabled
                            timeline.snapEnabled = playbackManager.snapEnabled
                        }

                        onCutClicked: {
                            if (!root.cutKeyPressed) {
                                root.cutKeyPressed = true
                                var t = playbackManager.currentTime
                                var selId = selectionManager.selectedClipId
                                if (DEBUG_MODE) {
                                    console.log("✂ Разрезать в позиции:", t,
                                                "clip:", selId)
                                }
                                if (cppTimeline) {
                                    if (selId >= 0) {
                                        // Режем выделенный клип по индексу
                                        cppTimeline.splitClip(selId, t)
                                    } else {
                                        // Нет выделения — ищем любой клип под playhead
                                        if (!cppTimeline.splitClipAt(t, 1))
                                            cppTimeline.splitClipAt(t, 2)
                                    }
                                }
                                cutDebounceTimer.restart()
                            }
                        }

                        onClearEffectsClicked: {
                            if (DEBUG_MODE)
                                console.log("Удалить эффекты")
                        }
                    }

                    // Timeline
                    Timeline {
                        id: timeline
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.timelineHeight

                        currentTime: playbackManager.currentTime
                        duration: playbackManager.duration
                        zoomLevel: playbackManager.zoomLevel
                        snapEnabled: playbackManager.snapEnabled

                        //  КЛЮЧЕВОЕ: пробрасываем selectedClipId вниз по цепочке
                        // main - Timeline.selectedClipId - Track.selectedClipId
                        //   - VideoClip.selected = (root.selectedClipId === modelData.id)
                        selectedClipId: selectionManager.selectedClipId
                        clipStates: clipStates

                        onTimeChanged: time => {
                                           playbackManager.currentTime = time
                                           cppTimeline.currentTime = time
                                           // Seek во время воспроизведения — перезапустить с новой позиции
                                           if (playbackManager.isPlaying) {
                                               cppTimeline.stopPlayback()
                                               cppTimeline.startPlayback(
                                                   time,
                                                   playbackManager.playbackSpeed)
                                           }
                                       }
                        onZoomChanged: zoom => {
                                           playbackManager.zoomLevel = zoom
                                       }

                        // *** Получаем выбор клипа снизу вверх: VideoClip - Track - Timeline - main ***
                        onClipSelected: id => {
                                            selectionManager.selectedClipId
                                            = (selectionManager.selectedClipId === id) ? -1 : id
                                            if (DEBUG_MODE) {
                                                console.log(
                                                    id >= 0 ? "✅ Выделен клип "
                                                              + id : "❌ Выделение снято")
                                            }
                                        }
                        onEffectsRequested: id => {
                                                clipEffectsDialog.openForClip(
                                                    id)
                                            }
                        //  ДОБАВИТЬ ЭТИ ОБРАБОТЧИКИ (после onEffectsRequested):
                        onShowVideoContextMenu: (clipId, track, clipName, x, y) => {
                                                    menuContext.clipId = clipId
                                                    menuContext.track = track
                                                    menuContext.clipName = clipName
                                                    var clips = cppTimeline ? cppTimeline.getClipsForTrack(track) : []
                                                    for (var i = 0; i < clips.length; i++) {
                                                        if (clips[i].id === clipId) {
                                                            menuContext.isMuted = clips[i].isMuted
                                                            || false
                                                            break
                                                        }
                                                    }
                                                    var lp = root.contentItem.mapFromGlobal(
                                                        x, y)
                                                    if (DEBUG_MODE) {
                                                        console.log(
                                                            ">>> POPUP x=",
                                                            lp.x, "y=",
                                                            lp.y, "w=",
                                                            videoContextMenu.width)
                                                    }
                                                    videoContextMenu.popup(
                                                        lp.x, lp.y)
                                                    if (DEBUG_MODE) {
                                                        console.log(
                                                            ">>> visible=",
                                                            videoContextMenu.visible)
                                                    }
                                                }

                        onShowAudioContextMenu: (clipId, track, clipName, isMuted, x, y) => {
                                                    menuContext.clipId = clipId
                                                    menuContext.track = track
                                                    menuContext.clipName = clipName
                                                    menuContext.isMuted = isMuted
                                                    var lp = root.contentItem.mapFromGlobal(
                                                        x, y)
                                                    audioContextMenu.popup(
                                                        lp.x, lp.y)
                                                }
                    }
                }
            }
        }

        // ГОРЯЧИЕ КЛАВИШИ
        // Все Shortcut находятся в Window root - cutKeyPressed доступен без проблем
        Shortcut {
            sequence: "Space"
            onActivated: playbackManager.isPlaying = !playbackManager.isPlaying
        }
        Shortcut {
            sequence: "K"
            onActivated: playbackManager.isPlaying = false
        }
        Shortcut {
            sequence: "J"
            onActivated: {
                playbackManager.currentTime = Math.max(
                            0,
                            playbackManager.currentTime - 5 * playbackManager.playbackSpeed)
                cppTimeline.currentTime = playbackManager.currentTime
            }
        }
        Shortcut {
            sequence: "L"
            onActivated: {
                playbackManager.currentTime = Math.min(
                            playbackManager.duration,
                            playbackManager.currentTime + 5 * playbackManager.playbackSpeed)
                cppTimeline.currentTime = playbackManager.currentTime
            }
        }
        Shortcut {
            sequence: "Ctrl+O"
            onActivated: openVideoDialog.open()
        }
        Shortcut {
            sequence: "Ctrl+S"
            onActivated: saveProjectDialog.open()
        }
        Shortcut {
            sequence: "Ctrl+E"
            onActivated: exportDialog.open()
        }
        Shortcut {
            sequence: "Home"
            onActivated: {
                playbackManager.currentTime = 0
                cppTimeline.currentTime = 0
            }
        }
        Shortcut {
            sequence: "End"
            onActivated: {
                playbackManager.currentTime = playbackManager.duration
                cppTimeline.currentTime = playbackManager.duration
            }
        }
        Shortcut {
            sequence: "Left"
            onActivated: {
                playbackManager.currentTime = Math.max(
                            0, playbackManager.currentTime - 0.033)
                cppTimeline.currentTime = playbackManager.currentTime
            }
        }
        Shortcut {
            sequence: "Right"
            onActivated: {
                playbackManager.currentTime = Math.min(
                            playbackManager.duration,
                            playbackManager.currentTime + 0.033)
                cppTimeline.currentTime = playbackManager.currentTime
            }
        }

        // Delete — удалить выделенный клип
        Shortcut {
            sequence: "Delete"
            onActivated: {
                var id = selectionManager.selectedClipId
                if (id >= 0 && cppTimeline) {
                    if (DEBUG_MODE)
                        console.log("🗑️ Delete выделенного клипа", id)
                    selectionManager.selectedClipId = -1
                    cppTimeline.removeClip(id)
                } else {
                    if (DEBUG_MODE)
                        console.log("ℹ️ Ничего не выделено для удаления")
                }
            }
        }

        // C — разрезать выделенный клип по playhead
        Shortcut {
            sequence: "C"
            onActivated: {
                if (!root.cutKeyPressed) {
                    root.cutKeyPressed = true
                    var t = playbackManager.currentTime
                    var selId = selectionManager.selectedClipId
                    if (DEBUG_MODE)
                        console.log("✂ Разрезать (C) at", t, "clip:", selId)
                    if (cppTimeline) {
                        if (selId >= 0) {
                            cppTimeline.splitClip(selId, t)
                        } else {
                            // Нет выделения — ищем любой клип под playhead
                            if (!cppTimeline.splitClipAt(t, 1))
                                cppTimeline.splitClipAt(t, 2)
                        }
                    }
                    cutDebounceTimer.restart()
                }
            }
        }

        // Escape — снять выделение
        Shortcut {
            sequence: "Escape"
            onActivated: selectionManager.clearSelection()
        }

        Shortcut {
            sequence: "Ctrl+Z"
            onActivated: {
                if (DEBUG_MODE)
                    console.log("Undo")
            }
        }
        Shortcut {
            sequence: "Ctrl+Y"
            onActivated: {
                if (DEBUG_MODE)
                    console.log("Redo")
            }
        }
        Shortcut {
            sequence: "M"
            onActivated: {
                if (DEBUG_MODE)
                    console.log("Marker at", playbackManager.currentTime)
            }
        }
        Shortcut {
            sequence: "+"
            onActivated: playbackManager.zoomLevel = Math.min(
                             500, playbackManager.zoomLevel + 20)
        }
        Shortcut {
            sequence: "-"
            onActivated: playbackManager.zoomLevel = Math.max(
                             10, playbackManager.zoomLevel - 20)
        }

        Timer {
            id: cutDebounceTimer
            interval: 200
            onTriggered: root.cutKeyPressed = false
        }
    } // Window
} // QtObject
