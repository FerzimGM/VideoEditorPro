import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: videoPlayer
    color: Theme.backgroundDark
    radius: Theme.borderRadius
    border.color: Theme.borderLight
    border.width: 1

    property real currentTime: 0
    property real duration: cppTimeline ? Math.max(
                                              60,
                                              cppTimeline.totalDuration) : 60

    // ===== ЭФФЕКТЫ В РЕАЛЬНОМ ВРЕМЕНИ =====
    property real brightness: 1.0 // 0.5 - 2.0
    property real contrast: 1.0 // 0.5 - 2.0
    property real saturation: 1.0 // 0.0 - 2.0
    property bool grayscale: false // Чёрно-белое

    // *** ИСПРАВЛЕНО: путь к кадру — строка, не QImage ***
    // QImage нельзя передавать в QML напрямую!
    // Используем C++ метод getFramePathAt() который возвращает "file:///path/to/frame.png"
    // ===== ИСТОЧНИК КАДРА =====
    property string currentFrameSource: ""

    // Метаданные текущего клипа (для отображения в UI)
    property int  videoWidth:  0
    property int  videoHeight: 0
    property real videoFps:    0.0

    function applyBrightness(value) {
        brightness = value
    }
    function applyContrast(value) {
        contrast = value
    }
    function applySaturation(value) {
        saturation = value
    }
    function applyGrayscale(enabled) {
        grayscale = enabled
    }
    function resetEffects() {
        brightness = 1.0
        contrast = 1.0
        saturation = 1.0
        grayscale = false
    }

    // ===== ОБНОВЛЕНИЕ КАДРА =====
    function updateFrame() {
        if (!cppTimeline) {
            console.log("⚠️ cppTimeline не доступен")
            currentFrameSource = ""
            return
        }

        // C++ возвращает путь "file:///C:/temp/frame_xxx.png" или ""
        var framePath = cppTimeline.getFramePathAt(currentTime, 1)

        if (framePath && framePath !== "") {
            // Добавляем "?" + timestamp чтобы QML Image перезагрузил файл
            // (без этого Image кеширует по пути и не обновляется)
            currentFrameSource = framePath + "?t=" + Date.now()
            console.log("✅ Кадр обновлён:", currentTime.toFixed(2), "сек")
        } else {
            currentFrameSource = ""
        }
    }

    // Авто-обновление при изменении времени
    onCurrentTimeChanged: {
        updateFrame()
    }

    // Также обновляемся при добавлении клипов
    Connections {
        target: cppTimeline
        function onClipsChanged() {
            videoPlayer.updateFrame()
        }
        function onTotalDurationChanged() {// duration пересчитается автоматически через binding
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

        // Область видео
        Rectangle {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            radius: Theme.borderRadius
            clip: true

            // ===== ОТОБРАЖЕНИЕ КАДРА =====
            Image {
                id: videoFrame
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                source: videoPlayer.currentFrameSource
                fillMode: Image.PreserveAspectFit
                // *** ИСПРАВЛЕНО: cache: false чтобы Image всегда перезагружал файл ***
                cache: false
                asynchronous: true
                visible: videoPlayer.currentFrameSource !== ""

                // ===== SHADER EFFECTS =====
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
uniform lowp float brightness;
uniform lowp float contrast;
uniform lowp float saturation;
uniform bool grayscale;
varying highp vec2 qt_TexCoord0;

void main() {
vec4 color = texture2D(source, qt_TexCoord0);
color.rgb *= brightness;
color.rgb = (color.rgb - 0.5) * contrast + 0.5;
float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
color.rgb = mix(vec3(gray), color.rgb, saturation);
if (grayscale) { color.rgb = vec3(gray); }
gl_FragColor = vec4(clamp(color.rgb, 0.0, 1.0), 1.0);
}
"
                }
            }

            // Placeholder когда нет видео
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingLarge
                visible: !videoFrame.visible

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
        }

        // ===== МЕТАДАННЫЕ (разрешение / FPS) =====
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLarge

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
                        text: videoFrame.visible
                                                     ? (videoPlayer.videoWidth > 0
                                                        ? videoPlayer.videoWidth + " × " + videoPlayer.videoHeight
                                                        : "— × —")
                                                     : "— × —"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

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
                        text: videoFrame.visible
                                                      ? (videoPlayer.videoFps > 0
                                                         ? videoPlayer.videoFps.toFixed(3) + " FPS"
                                                         : "— FPS")
                                                      : "— FPS"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }
    }

    function formatTime(seconds) {
        var hours = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = Math.floor(seconds % 60)
        if (hours > 0)
            return pad(hours) + ":" + pad(mins) + ":" + pad(secs)
        return pad(mins) + ":" + pad(secs)
    }

    function pad(num) {
        return num < 10 ? "0" + num : String(num)
    }
}
