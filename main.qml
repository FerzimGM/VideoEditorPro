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
        }
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
                    clipManager: clipManager  // Передаем менеджер клипов
                    
                    onTimeChanged: (time) => playbackManager.currentTime = time
                    onZoomChanged: (zoom) => playbackManager.zoomLevel = zoom
                }
            }
        }
    } // ColumnLayout

    // Горячие клавиши
    Shortcut {
        sequence: "Space"
        onActivated: playbackManager.isPlaying = !playbackManager.isPlaying
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
        sequence: "Home"
        onActivated: playbackManager.currentTime = 0
    }

    Shortcut {
        sequence: "End"
        onActivated: playbackManager.currentTime = playbackManager.duration
    }
    
    } // Window (mainWindow)
} // QtObject (appRoot)
