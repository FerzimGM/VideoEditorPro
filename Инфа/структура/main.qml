import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"
import "theme.js" as Theme

QtObject {
    id: appRoot

    // ===== ОКНО 1: SPLASH SCREEN =====
    property var splashWindow: Window {
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

    // ===== ГЛАВНОЕ ОКНО =====
    property var mainWindow: Window {
        id: root
        visible: false
        width: 1600
        height: 900
        minimumWidth: 1280
        minimumHeight: 720
        title: "VideoEditor Pro"
        color: Theme.backgroundColor
        flags: Qt.Window | Qt.FramelessWindowHint

        // *** cutKeyPressed объявлен здесь, в Window root ***
        // Shortcuts тоже в Window root → доступ без проблем
        property bool cutKeyPressed: false

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

        QtObject {
            id: playbackManager
            property real currentTime: 0
            property real duration: cppTimeline ? Math.max(
                                                      60,
                                                      cppTimeline.totalDuration) : 60
            property bool isPlaying: false
            property real playbackSpeed: 1.0
            property int zoomLevel: 100
            property bool snapEnabled: true
        }

        // ===== МЕНЕДЖЕР ВЫДЕЛЕНИЯ =====
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

        // ===== ДИАЛОГИ =====
        FileDialog {
            id: openVideoDialog
            title: "Открыть видео"
            nameFilters: ["Video files (*.mp4 *.avi *.mov *.mkv)", "All files (*)"]
            onAccepted: {
                var filepath = selectedFile.toString()
                filepath = filepath.replace(/^file:\/\/\//, "")
                if (filepath.match(/^\/[A-Za-z]:\//))
                    filepath = filepath.substring(1)

                // ***  добавляем в конец последнего клипа, не на текущее время ***
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

                console.log("Добавляю видео:", filepath, "startTime:",
                            startTime)
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
            nameFilters: ["MP4 (*.mp4)", "AVI (*.avi)", "MOV (*.mov)", "MKV (*.mkv)"]
            defaultSuffix: "mp4"
            onAccepted: {
                var filepath = selectedFile.toString()
                filepath = filepath.replace(/^file:\/\/\//, "")
                if (filepath.match(/^\/[A-Za-z]:\//))
                    filepath = filepath.substring(1)
                exportDialog.isRendering = true
                exportDialog.renderProgress = 0
                exportDialog.open()
                cppTimeline.renderToFile(filepath)
            }
        }

        Dialog {
            id: exportDialog
            title: "Экспорт видео"
            width: 500
            height: 400
            anchors.centerIn: parent
            modal: true
            property int renderProgress: 0
            property bool isRendering: false

            background: Rectangle {
                color: Theme.backgroundColor
                border.color: Theme.rubyPrimary
                border.width: 2
                radius: Theme.borderRadius
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingLarge

                Text {
                    text: exportDialog.isRendering ? "Рендеринг..." : "Настройки экспорта"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing
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
                }

                ProgressBar {
                    Layout.fillWidth: true
                    visible: exportDialog.isRendering
                    value: exportDialog.renderProgress / 100.0
                    background: Rectangle {
                        color: Theme.backgroundDark
                        radius: Theme.borderRadius
                    }
                    contentItem: Item {
                        Rectangle {
                            width: parent.width * exportDialog.renderProgress / 100.0
                            height: parent.height
                            radius: Theme.borderRadius
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop {
                                    position: 0.0
                                    color: Theme.rubyGradientStart
                                }
                                GradientStop {
                                    position: 1.0
                                    color: Theme.rubyGradientEnd
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: exportDialog.isRendering
                    text: exportDialog.renderProgress + "%"
                    color: Theme.rubyLight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.alignment: Qt.AlignHCenter
                }

                Button {
                    text: exportDialog.isRendering ? "Отмена" : "Экспортировать"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: exportDialog.isRendering ? exportDialog.close(
                                                              ) : exportFileDialog.open()
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
                    }
                }
            }

            Connections {
                target: cppTimeline
                function onRenderProgress(percent) {
                    exportDialog.renderProgress = percent
                }
                function onRenderFinished(success) {
                    exportDialog.isRendering = false
                    console.log(success ? "✅ Экспорт OK" : "❌ Ошибка")
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

        // ===== ГЛАВНЫЙ LAYOUT =====
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
                            // *** Передаём скорость → QMediaPlayer.playbackRate ***
                            playbackSpeed: playbackManager.playbackSpeed
                            volume: 1.0

                            // Обновляем playhead из QMediaPlayer
                            onTimePositionChanged: time => {
                                                       if (Math.abs(
                                                               playbackManager.currentTime
                                                               - time) > 0.05) {
                                                           playbackManager.currentTime = time
                                                           cppTimeline.currentTime = time
                                                       }
                                                   }
                            // Конец таймлайна — VideoPlayer не может напрямую
                            // ставить isPlaying=false (сломает QML binding).
                            // Он эмитирует playbackStopped() → мы сбрасываем через manager.
                            onPlaybackStopped: {
                                playbackManager.isPlaying = false
                                // Обновляем scrub frame после остановки
                                Qt.callLater(function () {
                                    cppTimeline.currentTime = playbackManager.currentTime
                                })
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

                        onPlayPauseClicked: playbackManager.isPlaying = !playbackManager.isPlaying

                        onStopClicked: {
                            playbackManager.isPlaying = false
                            playbackManager.currentTime = 0
                            // ***  cppTimeline.currentTime, не setCurrentTime ***
                            cppTimeline.currentTime = 0
                        }

                        onSeek: time => {
                                    playbackManager.currentTime = time
                                    cppTimeline.currentTime = time
                                }

                        onSpeedChanged: speed => {
                                            playbackManager.playbackSpeed = speed
                                        }
                        onSnapToggled: playbackManager.snapEnabled = !playbackManager.snapEnabled

                        onCutClicked: {
                            if (!root.cutKeyPressed) {
                                root.cutKeyPressed = true
                                var t = playbackManager.currentTime
                                var selId = selectionManager.selectedClipId
                                console.log("✂ Разрезать в позиции:", t,
                                            "clip:", selId)
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

                        onClearEffectsClicked: console.log("Удалить эффекты")
                    }

                    // Timeline — НИЖЕ PlaybackControls
                    Timeline {
                        id: timeline
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.timelineHeight

                        currentTime: playbackManager.currentTime
                        duration: playbackManager.duration
                        zoomLevel: playbackManager.zoomLevel
                        snapEnabled: playbackManager.snapEnabled

                        // *** КЛЮЧЕВОЕ: пробрасываем selectedClipId вниз по цепочке ***
                        // main → Timeline.selectedClipId → Track.selectedClipId
                        //   → VideoClip.selected = (root.selectedClipId === modelData.id)
                        selectedClipId: selectionManager.selectedClipId

                        onTimeChanged: time => {
                                           playbackManager.currentTime = time
                                           cppTimeline.currentTime = time
                                       }
                        onZoomChanged: zoom => {
                                           playbackManager.zoomLevel = zoom
                                       }

                        // *** Получаем выбор клипа снизу вверх: VideoClip → Track → Timeline → main ***
                        onClipSelected: id => {
                                            selectionManager.selectedClipId
                                            = (selectionManager.selectedClipId === id) ? -1 : id
                                            console.log(
                                                id >= 0 ? "✅ Выделен клип "
                                                          + id : "❌ Выделение снято")
                                        }
                        onEffectsRequested: id => {
                                                clipEffectsDialog.openForClip(
                                                    id)
                                            }
                    }
                }
            }
        }

        // ===== ГОРЯЧИЕ КЛАВИШИ =====
        // Все Shortcut находятся в Window root → cutKeyPressed доступен без проблем
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

        // *** Delete — удалить выделенный клип ***
        Shortcut {
            sequence: "Delete"
            onActivated: {
                var id = selectionManager.selectedClipId
                if (id >= 0 && cppTimeline) {
                    console.log("🗑️ Delete выделенного клипа", id)
                    selectionManager.selectedClipId = -1
                    cppTimeline.removeClip(id)
                } else {
                    console.log("ℹ️ Ничего не выделено для удаления")
                }
            }
        }

        // *** C — разрезать выделенный клип по playhead ***
        Shortcut {
            sequence: "C"
            onActivated: {
                if (!root.cutKeyPressed) {
                    root.cutKeyPressed = true
                    var t = playbackManager.currentTime
                    var selId = selectionManager.selectedClipId
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

        // *** Escape — снять выделение ***
        Shortcut {
            sequence: "Escape"
            onActivated: selectionManager.clearSelection()
        }

        Shortcut {
            sequence: "Ctrl+Z"
            onActivated: console.log("Undo")
        }
        Shortcut {
            sequence: "Ctrl+Y"
            onActivated: console.log("Redo")
        }
        Shortcut {
            sequence: "M"
            onActivated: console.log("Marker at", playbackManager.currentTime)
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
