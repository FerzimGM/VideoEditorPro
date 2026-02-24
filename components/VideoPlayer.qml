import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
// ← QMediaPlayer + AudioOutput + VideoOutput
import "../theme.js" as Theme

Rectangle {
    id: videoPlayer
    color: Theme.backgroundDark
    radius: Theme.borderRadius
    border.color: Theme.borderLight
    border.width: 1

    // ===== СВОЙСТВА =====
    property real currentTime: 0
    property real duration: cppTimeline ? Math.max(
                                              60,
                                              cppTimeline.totalDuration) : 60

    // *** ГЛАВНЫЙ ФИX: isPlaying должен приходить из main.qml! ***
    // Без него QMediaPlayer никогда не запускается и updateScrubFrame()
    // вызывается на каждом тике таймера → чёрный экран + спиннер
    property bool isPlaying: false
    property real playbackSpeed: 1.0 // ← скорость воспроизведения
    // Громкость (0.0 – 1.0)
    property real volume: 1.0

    // ===== ЭФФЕКТЫ =====
    property real brightness: 1.0
    property real contrast: 1.0
    property real saturation: 1.0
    property bool grayscale: false

    // ===== ВНУТРЕННИЕ ИСТОЧНИКИ =====
    property string currentClipUrl: ""
    property string currentFrameSource: ""

    // ===== МЕТАДАННЫЕ =====
    property int videoWidth: 0
    property int videoHeight: 0
    property real videoFps: 0.0

    // Сигнал: QMediaPlayer продвинулся — обновить playhead
    signal timePositionChanged(real newTime)

    // ===== ПУБЛИЧНЫЕ МЕТОДЫ =====
    function applyBrightness(v) {
        brightness = v
    }
    function applyContrast(v) {
        contrast = v
    }
    function applySaturation(v) {
        saturation = v
    }
    function applyGrayscale(v) {
        grayscale = v
    }
    function resetEffects() {
        brightness = 1.0
        contrast = 1.0
        saturation = 1.0
        grayscale = false
    }

    // ===== SCRUB FRAME (для паузы/скруббинга) =====
    function updateScrubFrame() {
        if (!cppTimeline) {
            currentFrameSource = ""
            return
        }

        // Статичный кадр через FFmpeg
        var fp = cppTimeline.getFramePathAt(currentTime, 1)
        currentFrameSource = (fp && fp !== "") ? fp + "?t=" + Date.now() : ""

        // Метаданные (разрешение, FPS) — из кэша, O(1)
        var info = cppTimeline.getClipInfoAt(currentTime, 1)
        if (info && info.width > 0) {
            videoWidth = info.width
            videoHeight = info.height
            videoFps = info.fps
        }

        // Путь для QMediaPlayer (готовим заранее, но play() не вызываем)
        var cp = cppTimeline.getActiveClipPath(currentTime, 1)
        if (cp && cp !== "" && currentClipUrl !== cp) {
            currentClipUrl = cp
            mediaPlayer.source = cp
        }
    }

    // ===== ВОСПРОИЗВЕДЕНИЕ =====
    function startPlayback() {
        // Убеждаемся что файл загружен
        var cp = cppTimeline ? cppTimeline.getActiveClipPath(currentTime,
                                                             1) : ""
        if (cp && cp !== "") {
            if (currentClipUrl !== cp) {
                currentClipUrl = cp
                mediaPlayer.source = cp
            }
        } else {
            return
            // Нет клипа — нечего играть
        }

        // Синхронизируем позицию QMediaPlayer с timeline
        // Синхронизируем позицию внутри файла
        var info = cppTimeline ? cppTimeline.getClipInfoAt(currentTime,
                                                           1) : null
        if (info) {
            var posMs = (currentTime - (info.startTime || 0) + (info.trimStart
                                                                || 0)) * 1000
            mediaPlayer.position = Math.max(0, posMs)
        }

        // *** Ключевое: применяем скорость воспроизведения ***
        mediaPlayer.playbackRate = videoPlayer.playbackSpeed
        mediaPlayer.play()
    }

    function pausePlayback() {
        mediaPlayer.pause()
        updateScrubFrame()
    }

    // ===== РЕАКЦИИ =====
    onIsPlayingChanged: {
        if (isPlaying)
            startPlayback()
        else
            pausePlayback()
    }

    // *** Скорость: применяем сразу при изменении ***
    onPlaybackSpeedChanged: {
        mediaPlayer.playbackRate = playbackSpeed
    }

    onCurrentTimeChanged: {
        if (!isPlaying) {
            updateScrubFrame()
        } else {
            // Проверяем drift при scrubbing во время воспроизведения
            var info = cppTimeline ? cppTimeline.getClipInfoAt(currentTime,
                                                               1) : null
            if (info) {
                var expectedMs = (currentTime - (info.startTime
                                                 || 0) + (info.trimStart
                                                          || 0)) * 1000
                if (Math.abs(expectedMs - mediaPlayer.position) > 1000) {
                    mediaPlayer.position = Math.max(0, expectedMs)
                }
            }
        }
    }

    onVolumeChanged: {
        audioOut.volume = volume
    }

    Connections {
        target: cppTimeline
        function onClipsChanged() {
            if (!videoPlayer.isPlaying)
                videoPlayer.updateScrubFrame()
        }
    }

    // ===== QMEDIAPLAYER =====
    MediaPlayer {
        id: mediaPlayer
        videoOutput: videoOutput
        audioOutput: AudioOutput {
            id: audioOut
            volume: videoPlayer.volume
        }

        onErrorOccurred: function (err, str) {
            console.log("❌ MediaPlayer error:", str)
        }

        // Двигаем playhead за QMediaPlayer
        onPositionChanged: {
            if (videoPlayer.isPlaying && mediaPlayer.source !== "") {
                var info = cppTimeline ? cppTimeline.getClipInfoAt(
                                             videoPlayer.currentTime, 1) : null
                if (info) {
                    var timelineTime = (mediaPlayer.position / 1000.0)
                            + (info.startTime || 0) - (info.trimStart || 0)
                    videoPlayer.timePositionChanged(timelineTime)
                }
            }
        }

        onPlaybackStateChanged: {
            if (mediaPlayer.playbackState === MediaPlayer.StoppedState
                    && videoPlayer.isPlaying) {
                videoPlayer.isPlaying = false
            }
        }
    }

    // Градиентная рамка
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.width: 2
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.rubyGradientStart
            }
            GradientStop {
                position: 0.5
                color: "transparent"
            }
            GradientStop {
                position: 1.0
                color: Theme.rubyGradientEnd
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacing

        // ===== ОБЛАСТЬ ВИДЕО =====
        Rectangle {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            radius: Theme.borderRadius
            clip: true

            // Режим воспроизведения: QMediaPlayer
            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                visible: videoPlayer.isPlaying && mediaPlayer.source !== ""

                layer.enabled: videoPlayer.brightness !== 1.0
                               || videoPlayer.contrast !== 1.0
                               || videoPlayer.saturation !== 1.0
                               || videoPlayer.grayscale
                layer.effect: ShaderEffect {
                    property real brightness: videoPlayer.brightness
                    property real contrast: videoPlayer.contrast
                    property real saturation: videoPlayer.saturation
                    property bool grayscale: videoPlayer.grayscale
                    fragmentShader: "
uniform lowp sampler2D source;
uniform lowp float brightness; uniform lowp float contrast;
uniform lowp float saturation; uniform bool grayscale;
varying highp vec2 qt_TexCoord0;
void main() {
vec4 c = texture2D(source, qt_TexCoord0);
c.rgb *= brightness;
c.rgb = (c.rgb - 0.5) * contrast + 0.5;
float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
c.rgb = mix(vec3(g), c.rgb, saturation);
if (grayscale) c.rgb = vec3(g);
gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);
}"
                }
            }

            // Режим паузы: FFmpeg кадр
            Image {
                id: scrubFrame
                anchors.fill: parent
                source: videoPlayer.currentFrameSource
                fillMode: Image.PreserveAspectFit
                cache: false
                asynchronous: true
                visible: !videoPlayer.isPlaying
                         && videoPlayer.currentFrameSource !== ""

                BusyIndicator {
                    anchors.centerIn: parent
                    running: scrubFrame.status === Image.Loading
                    visible: running
                    width: 32
                    height: 32
                    palette.dark: Theme.rubyPrimary
                }

                layer.enabled: videoPlayer.brightness !== 1.0
                               || videoPlayer.contrast !== 1.0
                               || videoPlayer.saturation !== 1.0
                               || videoPlayer.grayscale
                layer.effect: ShaderEffect {
                    property real brightness: videoPlayer.brightness
                    property real contrast: videoPlayer.contrast
                    property real saturation: videoPlayer.saturation
                    property bool grayscale: videoPlayer.grayscale
                    fragmentShader: "
uniform lowp sampler2D source;
uniform lowp float brightness; uniform lowp float contrast;
uniform lowp float saturation; uniform bool grayscale;
varying highp vec2 qt_TexCoord0;
void main() {
vec4 c = texture2D(source, qt_TexCoord0);
c.rgb *= brightness;
c.rgb = (c.rgb - 0.5) * contrast + 0.5;
float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
c.rgb = mix(vec3(g), c.rgb, saturation);
if (grayscale) c.rgb = vec3(g);
gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);
}"
                }
            }

            // Placeholder
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingLarge
                visible: !videoOutput.visible && !scrubFrame.visible
                Text {
                    text: "▶"
                    color: Theme.rubyPrimary
                    font.pixelSize: 72
                    opacity: 0.3
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: "Видеоплеер"
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLarge
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: "Добавьте видео на таймлайн"
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Временной код
            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.spacing
                width: timecodeText.width + Theme.spacing * 2
                height: timecodeText.height + Theme.spacingSmall * 2
                color: Qt.rgba(0, 0, 0, 0.7)
                radius: Theme.borderRadius
                Text {
                    id: timecodeText
                    anchors.centerIn: parent
                    text: formatTime(
                              videoPlayer.currentTime) + " / " + formatTime(
                              videoPlayer.duration)
                    color: Theme.rubyLight
                    font.family: "Consolas, monospace"
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                }
            }

            // Индикатор скорости (если не 1.0x)
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.spacing
                width: speedLabel.width + 12
                height: speedLabel.height + 6
                radius: Theme.borderRadius
                color: Qt.rgba(0, 0, 0, 0.7)
                visible: videoPlayer.playbackSpeed !== 1.0
                Text {
                    id: speedLabel
                    anchors.centerIn: parent
                    text: videoPlayer.playbackSpeed + "x"
                    color: Theme.rubyPrimary
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                }
            }
        }

        // ===== МЕТАДАННЫЕ + ГРОМКОСТЬ =====
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge

            // Разрешение
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: "Разрешение"
                        color: Theme.rubyLight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: videoPlayer.videoWidth > 0 ? videoPlayer.videoWidth + " × "
                                                           + videoPlayer.videoHeight : "— × —"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // FPS
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSmall
                    Text {
                        text: "Частота кадров"
                        color: Theme.rubyLight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: videoPlayer.videoFps > 0 ? videoPlayer.videoFps.toFixed(
                                                             3) + " FPS" : "— FPS"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Громкость
            Rectangle {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 60
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: videoPlayer.volume
                                  === 0 ? "🔇" : (videoPlayer.volume < 0.5 ? "🔉" : "🔊")
                            font.pixelSize: 16
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: videoPlayer.volume = videoPlayer.volume > 0 ? 0 : 1.0
                            }
                        }
                        Text {
                            text: "Громкость"
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        Text {
                            text: Math.round(videoPlayer.volume * 100) + "%"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                    Slider {
                        id: volumeSlider
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: videoPlayer.volume
                        stepSize: 0.01
                        onValueChanged: videoPlayer.volume = value
                        background: Rectangle {
                            x: volumeSlider.leftPadding
                            y: volumeSlider.topPadding + (volumeSlider.availableHeight - height) / 2
                            width: volumeSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.backgroundDark
                            border.color: Theme.borderLight
                            border.width: 1
                            Rectangle {
                                width: volumeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: parent.radius
                                color: Theme.rubyPrimary
                            }
                        }
                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition
                               * (volumeSlider.availableWidth - width)
                            y: volumeSlider.topPadding + (volumeSlider.availableHeight - height) / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: Theme.rubyLight
                            border.color: Theme.rubyPrimary
                            border.width: 1
                        }
                    }
                }
            }
        }
    }

    function formatTime(seconds) {
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = Math.floor(seconds % 60)
        if (h > 0)
            return pad(h) + ":" + pad(m) + ":" + pad(s)
        return pad(m) + ":" + pad(s)
    }
    function pad(n) {
        return n < 10 ? "0" + n : String(n)
    }
}
