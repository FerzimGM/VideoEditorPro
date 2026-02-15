import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.backgroundDark
    radius: Theme.borderRadius
    border.color: Theme.borderLight
    border.width: 1

    property real currentTime: 0
    property real duration: 100
    
    // ===== ЭФФЕКТЫ В РЕАЛЬНОМ ВРЕМЕНИ =====
    property real brightness: 1.0      // 0.5 - 2.0
    property real contrast: 1.0        // 0.5 - 2.0
    property real saturation: 1.0      // 0.0 - 2.0
    property bool grayscale: false     // Чёрно-белое
    
    // Функции для применения эффектов (вызываются из LeftSidebar)
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

    // Градиентная рамка
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 0.5; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacing

        // Область видео с ShaderEffect
        Rectangle {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            radius: Theme.borderRadius
            
            // ===== SHADER EFFECTS =====
            layer.enabled: root.brightness !== 1.0 || root.contrast !== 1.0 || 
                          root.saturation !== 1.0 || root.grayscale
            layer.effect: ShaderEffect {
                property real brightness: root.brightness
                property real contrast: root.contrast
                property real saturation: root.saturation
                property bool grayscale: root.grayscale
                
                fragmentShader: "
                    uniform lowp sampler2D source;
                    uniform lowp float brightness;
                    uniform lowp float contrast;
                    uniform lowp float saturation;
                    uniform bool grayscale;
                    varying highp vec2 qt_TexCoord0;
                    
                    void main() {
                        vec4 color = texture2D(source, qt_TexCoord0);
                        
                        // Яркость
                        color.rgb *= brightness;
                        
                        // Контраст
                        color.rgb = (color.rgb - 0.5) * contrast + 0.5;
                        
                        // Насыщенность
                        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
                        color.rgb = mix(vec3(gray), color.rgb, saturation);
                        
                        // Ч/Б
                        if (grayscale) {
                            color.rgb = vec3(gray);
                        }
                        
                        gl_FragColor = vec4(color.rgb, 1.0);
                    }
                "
            }

            // Placeholder для видео
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacingLarge

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
                    text: "Откройте видео для просмотра"
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.alignment: Qt.AlignHCenter
                }
                
                // Индикатор эффектов
                Text {
                    visible: root.brightness !== 1.0 || root.contrast !== 1.0 || 
                            root.saturation !== 1.0 || root.grayscale
                    text: "✨ Эффекты применены"
                    color: Theme.rubyLight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
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
                    text: formatTime(root.currentTime) + " / " + formatTime(root.duration)
                    color: Theme.rubyLight
                    font.family: "Consolas, monospace"
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                }
            }
        }

        // Информация о видео - ЯРКАЯ
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
                        text: "1920 × 1080"
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
                        text: "30 FPS"
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
                        text: "Битрейт"
                        color: Theme.rubyLight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "8 Mbps"
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
        var mins = Math.floor(seconds / 60)
        var secs = Math.floor(seconds % 60)
        var frames = Math.floor((seconds % 1) * 30)
        return pad(mins) + ":" + pad(secs) + ":" + pad(frames)
    }

    function pad(num) {
        return num < 10 ? "0" + num : num
    }
}
