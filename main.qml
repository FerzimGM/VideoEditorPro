import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"
import "theme.js" as Theme

QtObject {
    id: appRoot

    // ===== ОКНО 1: SPLASH SCREEN (показывается первым) =====
    property var splashWindow: Window {
        id: splashWin
        visible: true  // Показываем сразу!
        width: 800
        height: 435
        color: "transparent"
        flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

        // Центрируем на экране
        Component.onCompleted: {
            x = (Screen.width - width) / 2
            y = (Screen.height - height) / 2
        }

        // Затемнённый фон
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.95
            radius: Theme.borderRadius
        }

        // SplashScreen компонент
        Rectangle {
            anchors.fill: parent
            color: Theme.backgroundColor
            radius: Theme.borderRadius
            border.color: Theme.rubyPrimary
            border.width: 2

            SplashScreen {
                anchors.fill: parent

                onLoaded: {
                    // Закрываем splash и показываем главное окно
                    splashWin.close()
                    mainWindow.visible = true
                }
            }
        }
    }

    // ===== ОКНО 2: ГЛАВНОЕ ОКНО (скрыто до окончания загрузки) =====
    property var mainWindow: Window {
        id: root
        visible: false  // Скрыто пока идёт загрузка!
        width: 1600
        height: 900
        minimumWidth: 1280
        minimumHeight: 720
        title: "VideoEditor Pro"
        color: Theme.backgroundColor
        flags: Qt.Window | Qt.FramelessWindowHint

    // Resize handles для frameless окна
    // Правый край
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        z: 1000

        property real startMouseX: 0
        property real startWidth: 0

        onPressed: (mouse) => {
            startMouseX = mouseX + root.x + root.width - 5
            startWidth = root.width
        }

        onMouseXChanged: {
            if (pressed) {
                var currentMouseX = mouseX + root.x + root.width - 5
                var delta = currentMouseX - startMouseX
                root.width = Math.max(root.minimumWidth, startWidth + delta)
            }
        }
    }

    // Нижний край
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 5
        cursorShape: Qt.SizeVerCursor
        z: 1000

        property real startMouseY: 0
        property real startHeight: 0

        onPressed: (mouse) => {
            startMouseY = mouseY + root.y + root.height - 5
            startHeight = root.height
        }

        onMouseYChanged: {
            if (pressed) {
                var currentMouseY = mouseY + root.y + root.height - 5
                var delta = currentMouseY - startMouseY
                root.height = Math.max(root.minimumHeight, startHeight + delta)
            }
        }
    }

    // Правый нижний угол
    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 10
        height: 10
        cursorShape: Qt.SizeFDiagCursor
        z: 1001

        property real startMouseX: 0
        property real startMouseY: 0
        property real startWidth: 0
        property real startHeight: 0

        onPressed: (mouse) => {
            startMouseX = mouseX + root.x + root.width - 10
            startMouseY = mouseY + root.y + root.height - 10
            startWidth = root.width
            startHeight = root.height
        }

        onPositionChanged: (mouse) => {
            if (pressed) {
                var currentMouseX = mouse.x + root.x + root.width - 10
                var currentMouseY = mouse.y + root.y + root.height - 10
                var deltaX = currentMouseX - startMouseX
                var deltaY = currentMouseY - startMouseY
                root.width = Math.max(root.minimumWidth, startWidth + deltaX)
                root.height = Math.max(root.minimumHeight, startHeight + deltaY)
            }
        }
    }

    // Менеджер проекта
    QtObject {
        id: projectManager
        property string currentProjectPath: ""
        property bool isProjectModified: false
        property var clips: []
    }

    // Менеджер клипов (пока фронтенд, позже подключим к C++)
    ClipManager {
        id: clipManager

        // Когда добавится бэкенд, здесь будут сигналы:
        // onClipAdded: cppTimeline.addClip(filepath, trackNumber, startTime)
        // onClipMoved: cppTimeline.moveClip(clipId, newTrack, newTime)
        // onClipRemoved: cppTimeline.removeClip(clipId)
    }

    // Менеджер воспроизведения
    QtObject {
        id: playbackManager
        property real currentTime: 0
        property real duration: 100
        property bool isPlaying: false
        property real playbackSpeed: 1.0
        property int zoomLevel: 100
        property bool snapEnabled: true
    }

    // Диалог открытия видео
    FileDialog {
        id: openVideoDialog
        title: "Открыть видео"
        nameFilters: ["Video files (*.mp4 *.avi *.mov *.mkv)", "All files (*)"]
        onAccepted: {
            var filepath = selectedFile.toString()
            // Убираем file:/// prefix если есть
            filepath = filepath.replace(/^file:\/\/\//, "")
            console.log("Выбрано видео:", filepath)
            // Добавляем клип на первую дорожку в текущую позицию playhead
            clipManager.addClip(filepath, 1, playbackManager.currentTime, 10.0)
        }
    }

    // Диалог открытия проекта
    FileDialog {
        id: openProjectDialog
        title: "Открыть проект"
        nameFilters: ["Project files (*.vep)", "All files (*)"]
        onAccepted: {
            console.log("Открыт проект:", selectedFile)
            projectManager.currentProjectPath = selectedFile
        }
    }

    // Диалог сохранения проекта
    FileDialog {
        id: saveProjectDialog
        fileMode: FileDialog.SaveFile
        title: "Сохранить проект"
        nameFilters: ["Project files (*.vep)", "All files (*)"]
        defaultSuffix: "vep"
        onAccepted: {
            console.log("Сохранен проект:", selectedFile)
            projectManager.currentProjectPath = selectedFile
            projectManager.isProjectModified = false
            // TODO: cppTimeline.saveProject(selectedFile)
        }
    }

    // ===== ДИАЛОГ ЭКСПОРТА ВИДЕО =====
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

            // Заголовок
            Text {
                text: exportDialog.isRendering ? "Рендеринг..." : "Настройки экспорта"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // Настройки (показываем только когда НЕ рендерим)
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing
                visible: !exportDialog.isRendering

                Text {
                    text: "Формат"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }

                ComboBox {
                    id: formatCombo
                    Layout.fillWidth: true
                    model: ["MP4 (H.264)", "AVI", "MOV", "MKV", "WebM"]
                    currentIndex: 0

                    contentItem: Text {
                        text: parent.displayText
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacing
                    }

                    background: Rectangle {
                        color: parent.down ? Theme.buttonPressed : (parent.hovered ? Theme.buttonHover : Theme.buttonBackground)
                        radius: Theme.borderRadius
                        border.color: Theme.rubyPrimary
                        border.width: 1
                    }
                }

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
                    currentIndex: 0

                    contentItem: Text {
                        text: parent.displayText
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacing
                    }

                    background: Rectangle {
                        color: parent.down ? Theme.buttonPressed : (parent.hovered ? Theme.buttonHover : Theme.buttonBackground)
                        radius: Theme.borderRadius
                        border.color: Theme.rubyPrimary
                        border.width: 1
                    }
                }

                Text {
                    text: "Качество"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }

                Slider {
                    id: qualitySlider
                    Layout.fillWidth: true
                    from: 1
                    to: 10
                    value: 7
                    stepSize: 1
                    snapMode: Slider.SnapAlways

                    background: Rectangle {
                        x: parent.leftPadding
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        width: parent.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.backgroundDark

                        Rectangle {
                            width: parent.parent.visualPosition * parent.width
                            height: parent.height
                            color: Theme.rubyPrimary
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width / 2
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        width: 18
                        height: 18
                        radius: 9
                        color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary
                    }
                }

                Text {
                    text: "Качество: " + qualitySlider.value + "/10"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Прогресс бар (показываем при рендеринге)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingLarge
                visible: exportDialog.isRendering

                Text {
                    text: exportDialog.renderProgress + "%"
                    color: Theme.rubyPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 48
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                ProgressBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 20
                    value: exportDialog.renderProgress / 100

                    background: Rectangle {
                        color: Theme.backgroundDark
                        radius: Theme.borderRadius
                        border.color: Theme.rubyPrimary
                        border.width: 1
                    }

                    contentItem: Item {
                        Rectangle {
                            width: parent.parent.visualPosition * parent.width
                            height: parent.height
                            radius: Theme.borderRadius
                            color: Theme.rubyPrimary
                        }
                    }
                }

                Text {
                    text: "Рендеринг видео..."
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            Item { Layout.fillHeight: true }

            // Кнопки
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                Button {
                    text: exportDialog.isRendering ? "Отмена" : "Закрыть"
                    Layout.fillWidth: true

                    onClicked: {
                        if (exportDialog.isRendering) {
                            // TODO: cppTimeline.cancelRender()
                            console.log("Отмена рендеринга")
                        }
                        exportDialog.close()
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.down ? Theme.backgroundDark : (parent.hovered ? Theme.buttonHover : Theme.buttonBackground)
                        radius: Theme.borderRadius
                        border.color: Theme.borderLight
                        border.width: 1
                    }
                }

                Button {
                    text: "Экспорт"
                    Layout.fillWidth: true
                    visible: !exportDialog.isRendering

                    onClicked: {
                        // TODO: Выбор пути сохранения
                        // FileDialog для выбора выходного файла
                        // TODO: cppTimeline.renderToFile(outputPath, format, resolution, quality)

                        // Демо: симулируем рендеринг
                        exportDialog.isRendering = true
                        exportDialog.renderProgress = 0
                        renderSimulationTimer.start()
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
                    }
                }
            }
        }

        // Таймер для симуляции рендеринга (DEMO)
        Timer {
            id: renderSimulationTimer
            interval: 100
            repeat: true
            onTriggered: {
                exportDialog.renderProgress += 2
                if (exportDialog.renderProgress >= 100) {
                    stop()
                    exportDialog.isRendering = false
                    exportDialog.renderProgress = 0
                    console.log("✅ Рендеринг завершён!")
                }
            }
        }

        // TODO: Подключение к C++
        // Connections {
        //     target: cppTimeline
        //     function onRenderProgress(percent) {
        //         exportDialog.renderProgress = percent
        //     }
        //     function onRenderFinished(success) {
        //         exportDialog.isRendering = false
        //         if (success) {
        //             console.log("✅ Экспорт успешен!")
        //         }
        //     }
        // }
    }

    // Таймер для воспроизведения
    Timer {
        id: playbackTimer
        interval: 33 // ~30 FPS
        running: playbackManager.isPlaying
        repeat: true
        onTriggered: {
            playbackManager.currentTime += (interval / 1000.0) * playbackManager.playbackSpeed
            if (playbackManager.currentTime >= playbackManager.duration) {
                playbackManager.currentTime = playbackManager.duration
                playbackManager.isPlaying = false
            }
        }
    }

    // Верхняя панель меню
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
        onExportVideo: exportDialog.open()  // Открываем диалог экспорта!
        onMinimize: root.showMinimized()
        onMaximize: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        onClose: Qt.quit()
    }

    // Главный layout
    ColumnLayout {
        anchors.top: menuBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        // Основная рабочая область
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Левая панель с эффектами
            LeftSidebar {
                id: leftSidebar
                Layout.preferredWidth: Theme.sidebarWidth
                Layout.fillHeight: true
                videoPlayer: videoPlayer  // Связываем с VideoPlayer!
            }

            // Центральная область
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Область просмотра и управления
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.maximumHeight: root.height - Theme.timelineHeight - Theme.panelHeight - 20
                    spacing: Theme.spacing

                    // Переключатель режимов
                    ModeSwitcher {
                        id: modeSwitcher
                        Layout.preferredWidth: 180
                        Layout.fillHeight: true
                        Layout.margins: Theme.spacing

                        onModeChanged: (mode) => {
                            leftSidebar.currentMode = mode
                        }
                    }

                    // Видеоплеер
                    VideoPlayer {
                        id: videoPlayer
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: Theme.spacing

                        currentTime: playbackManager.currentTime
                        duration: playbackManager.duration
                    }
                }

                // Панель управления воспроизведением
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
                    }
                    onSeek: (time) => playbackManager.currentTime = time
                    onSpeedChanged: (speed) => playbackManager.playbackSpeed = speed
                    onSnapToggled: playbackManager.snapEnabled = !playbackManager.snapEnabled
                    onCutClicked: console.log("Разрезать клип в позиции:", playbackManager.currentTime)
                    onClearEffectsClicked: console.log("Удалить эффекты с выбранного клипа")
                }

                // Таймлайн
                Timeline {
                    id: timeline
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.timelineHeight

                    currentTime: playbackManager.currentTime
                    duration: playbackManager.duration
                    zoomLevel: playbackManager.zoomLevel
                    snapEnabled: playbackManager.snapEnabled  // Передаём snap
                    clipManager: clipManager

                    onTimeChanged: (time) => playbackManager.currentTime = time
                    onZoomChanged: (zoom) => playbackManager.zoomLevel = zoom
                }
            }
        }
    } // ColumnLayout

    // ===== ГОРЯЧИЕ КЛАВИШИ (как в Adobe Premiere Pro) =====

    // Воспроизведение
    Shortcut {
        sequence: "Space"
        onActivated: playbackManager.isPlaying = !playbackManager.isPlaying
    }

    Shortcut {
        sequence: "K"
        onActivated: playbackManager.isPlaying = false  // Пауза
    }

    Shortcut {
        sequence: "J"
        onActivated: {
            // Перемотка назад (5 сек × скорость)
            var rewind = 5 * playbackManager.playbackSpeed
            playbackManager.currentTime = Math.max(0, playbackManager.currentTime - rewind)
        }
    }

    Shortcut {
        sequence: "L"
        onActivated: {
            // Перемотка вперёд (5 сек × скорость)
            var forward = 5 * playbackManager.playbackSpeed
            playbackManager.currentTime = Math.min(playbackManager.duration,
                playbackManager.currentTime + forward)
        }
    }

    // Файлы
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: openVideoDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+S"
        onActivated: saveProjectDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        onActivated: {
            // TODO: Save As
            console.log("Save As...")
        }
    }

    Shortcut {
        sequence: "Ctrl+E"
        onActivated: exportDialog.open()
    }

    // Навигация
    Shortcut {
        sequence: "Home"
        onActivated: playbackManager.currentTime = 0
    }

    Shortcut {
        sequence: "End"
        onActivated: playbackManager.currentTime = playbackManager.duration
    }

    Shortcut {
        sequence: "Left"
        onActivated: {
            // Покадровая перемотка назад
            playbackManager.currentTime = Math.max(0, playbackManager.currentTime - 0.033)  // ~1 кадр
        }
    }

    Shortcut {
        sequence: "Right"
        onActivated: {
            // Покадровая перемотка вперёд
            playbackManager.currentTime = Math.min(playbackManager.duration,
                playbackManager.currentTime + 0.033)  // ~1 кадр
        }
    }

    // Редактирование (подготовка под C++)
    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: {
            // TODO: cppTimeline.undo()
            console.log("Undo")
        }
    }

    Shortcut {
        sequence: "Ctrl+Y"
        onActivated: {
            // TODO: cppTimeline.redo()
            console.log("Redo")
        }
    }

    Shortcut {
        sequence: "Delete"
        onActivated: {
            // TODO: Удалить выбранный клип
            console.log("Delete selected clip")
        }
    }

    Shortcut {
        sequence: "C"
        onActivated: {
            // TODO: Razor tool (разрезать)
            console.log("Cut at playhead")
        }
    }

    // Маркеры (подготовка)
    Shortcut {
        sequence: "M"
        onActivated: {
            // TODO: Добавить маркер
            console.log("Add marker at", playbackManager.currentTime)
        }
    }

    Shortcut {
        sequence: "Shift+M"
        onActivated: {
            // TODO: Перейти к следующему маркеру
            console.log("Go to next marker")
        }
    }

    // Zoom Timeline
    Shortcut {
        sequence: "+"
        onActivated: {
            playbackManager.zoomLevel = Math.min(500, playbackManager.zoomLevel + 20)
        }
    }

    Shortcut {
        sequence: "-"
        onActivated: {
            playbackManager.zoomLevel = Math.max(10, playbackManager.zoomLevel - 20)
        }
    }


    } // Window (mainWindow)
} // QtObject (appRoot)
