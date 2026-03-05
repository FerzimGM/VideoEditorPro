import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground

    property int currentMode: 0
    property var videoPlayer: null
    property string selectedEffect: ""

    // Сигнал экспорта
    signal exportRequested(string resolution, string format)

    // ── Выбранный клип + значения эффектов ──
    property int selectedClipId: -1

    property real effectBrightness: 0.0
    property real effectContrast:   1.0
    property real effectSaturation: 1.0
    property bool effectGrayscale:  false
    property real effectBlur:       0.0
    property real effectSharpness:  0.0
    property real effectVolume:     1.0
    property real effectHue:        0.0
    property real effectSepia:      0.0
    property real effectVignette:   0.0
    property real effectReverb:     0.0
    property real effectEcho:       0.0
    // Новые видео
    property bool effectInvert:     false
    property real effectPosterize:  0.0
    property real effectPixelate:   0.0
    property real effectTemperature:0.0
    property real effectTintHue:    0.0
    property real effectTintStr:    0.0
    // Новые аудио
    property bool effectMono:       false
    property real effectStereoWiden:0.0
    property real effectPitch:      0.0
    property real effectNormalize:  0.0
    property real effectFadeIn:     0.0
    property real effectFadeOut:    0.0

    function loadEffectsFromClip(clipId) {
        if (clipId < 0 || !cppTimeline) {
            effectBrightness = 0.0; effectContrast = 1.0; effectSaturation = 1.0
            effectGrayscale = false; effectBlur = 0.0; effectSharpness = 0.0; effectVolume = 1.0
            effectHue = 0.0; effectSepia = 0.0; effectVignette = 0.0; effectReverb = 0.0; effectEcho = 0.0
            effectInvert = false; effectPosterize = 0.0; effectPixelate = 0.0
            effectTemperature = 0.0; effectTintHue = 0.0; effectTintStr = 0.0
            effectMono = false; effectStereoWiden = 0.0; effectPitch = 0.0
            effectNormalize = 0.0; effectFadeIn = 0.0; effectFadeOut = 0.0
            return
        }
        var fx = cppTimeline.getClipEffects(clipId)
        effectBrightness = fx["brightness"] !== undefined ? fx["brightness"] : 0.0
        effectContrast   = fx["contrast"]   !== undefined ? fx["contrast"]   : 1.0
        effectSaturation = fx["saturation"] !== undefined ? fx["saturation"] : 1.0
        effectGrayscale  = fx["grayscale"]  !== undefined ? fx["grayscale"] > 0.5 : false
        effectBlur       = fx["blur"]       !== undefined ? fx["blur"]       : 0.0
        effectSharpness  = fx["sharpness"]  !== undefined ? fx["sharpness"]  : 0.0
        effectVolume     = fx["volume"]     !== undefined ? fx["volume"]     : 1.0
        effectHue        = fx["hue"]        !== undefined ? fx["hue"]        : 0.0
        effectSepia      = fx["sepia"]      !== undefined ? fx["sepia"]      : 0.0
        effectVignette   = fx["vignette"]   !== undefined ? fx["vignette"]   : 0.0
        effectReverb     = fx["reverb"]     !== undefined ? fx["reverb"]     : 0.0
        effectEcho       = fx["echo"]       !== undefined ? fx["echo"]       : 0.0
        effectInvert     = fx["invert"]    !== undefined ? fx["invert"] > 0.5  : false
        effectPosterize  = fx["posterize"] !== undefined ? fx["posterize"]     : 0.0
        effectPixelate   = fx["pixelate"]  !== undefined ? fx["pixelate"]      : 0.0
        effectTemperature= fx["temperature"]!== undefined ? fx["temperature"]  : 0.0
        effectTintHue    = fx["tint_hue"]  !== undefined ? fx["tint_hue"]      : 0.0
        effectTintStr    = fx["tint_strength"]!==undefined? fx["tint_strength"] : 0.0
        effectMono       = fx["mono"]      !== undefined ? fx["mono"] > 0.5    : false
        effectStereoWiden= fx["stereo_widen"]!==undefined? fx["stereo_widen"]  : 0.0
        effectPitch      = fx["pitch"]     !== undefined ? fx["pitch"]         : 0.0
        effectNormalize  = fx["normalize"] !== undefined ? fx["normalize"]     : 0.0
        effectFadeIn     = fx["fade_in"]   !== undefined ? fx["fade_in"]       : 0.0
        effectFadeOut    = fx["fade_out"]  !== undefined ? fx["fade_out"]      : 0.0
    }

    onSelectedClipIdChanged: loadEffectsFromClip(selectedClipId)

    // Боковая полоска
    Rectangle {
        anchors.right: parent.right
        width: 2
        height: parent.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════ РЕЖИМ ЭФФЕКТОВ ══════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentMode === 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: Theme.spacing

                Text {
                    text: "ПАНЕЛЬ ИНСТРУМЕНТОВ"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    Layout.fillWidth: true
                }

                // Параметры эффекта — верхняя треть
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: parent.height * 0.33
                    color: Theme.backgroundDark
                    radius: Theme.borderRadius
                    border.color: Theme.rubyPrimary
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing
                        spacing: Theme.spacing

                        Text {
                            text: "ПАРАМЕТРЫ ЭФФЕКТА"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                contentItem: Rectangle {
                                    implicitWidth: 6; radius: 3
                                    color: Theme.rubyPrimary
                                    opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                                }
                            }

                            ColumnLayout {
                                width: parent.width - 20
                                spacing: Theme.spacing

                                Text {
                                    text: root.selectedEffect || "Выберите эффект"
                                    color: root.selectedEffect ? Theme.rubyPrimary : Theme.textDisabled
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLarge
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Text {
                                    visible: root.selectedEffect !== "" && root.selectedClipId < 0
                                    text: "⚠️ Выберите клип на таймлайне"
                                    color: "#FFA726"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                Rectangle {
                                    Layout.fillWidth: true; height: 1
                                    color: Theme.dividerColor
                                    visible: root.selectedEffect !== ""
                                }

                                // ЯРКОСТЬ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Яркость"
                                    Text { text: "Яркость: " + (root.effectBrightness >= 0 ? "+" : "") + root.effectBrightness.toFixed(2); color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: -1.0; to: 1.0; stepSize: 0.05; value: root.effectBrightness
                                        onMoved: root.effectBrightness = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // КОНТРАСТ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Контраст"
                                    Text { text: "Контраст: " + root.effectContrast.toFixed(2) + "x"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 3.0; stepSize: 0.05; value: root.effectContrast
                                        onMoved: root.effectContrast = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // НАСЫЩЕННОСТЬ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Насыщенность"
                                    Text { text: "Насыщенность: " + root.effectSaturation.toFixed(2) + "x"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 2.0; stepSize: 0.05; value: root.effectSaturation
                                        onMoved: root.effectSaturation = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                    CheckBox {
                                        text: "Чёрно-белое"; checked: root.effectGrayscale
                                        onToggled: root.effectGrayscale = checked
                                        contentItem: Text { text: parent.text; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter }
                                        indicator: Rectangle { width: 20; height: 20; radius: 3; color: parent.checked ? Theme.rubyPrimary : Theme.backgroundDark; border.color: Theme.rubyPrimary; border.width: 2; Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 16; font.bold: true; visible: parent.parent.checked } }
                                    }
                                }

                                // РАЗМЫТИЕ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Размытие"
                                    Text { text: "Радиус: " + root.effectBlur.toFixed(1); color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 10.0; stepSize: 0.5; value: root.effectBlur
                                        onMoved: root.effectBlur = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // РЕЗКОСТЬ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Резкость"
                                    Text { text: "Резкость: " + root.effectSharpness.toFixed(1); color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 5.0; stepSize: 0.5; value: root.effectSharpness
                                        onMoved: root.effectSharpness = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ГРОМКОСТЬ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Громкость"
                                    Text { text: "Громкость: " + Math.round(root.effectVolume * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 2.0; stepSize: 0.05; value: root.effectVolume
                                        onMoved: root.effectVolume = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }


                                // ОТТЕНОК
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Оттенок"
                                    Text { text: "Сдвиг оттенка: " + root.effectHue.toFixed(0) + "°"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: -180; to: 180; stepSize: 1; value: root.effectHue
                                        onMoved: root.effectHue = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // СЕПИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Сепия"
                                    Text { text: "Интенсивность: " + Math.round(root.effectSepia * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectSepia
                                        onMoved: root.effectSepia = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ВИНЬЕТКА
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Виньетка"
                                    Text { text: "Интенсивность: " + Math.round(root.effectVignette * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectVignette
                                        onMoved: root.effectVignette = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // РЕВЕРБЕРАЦИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Реверберация"
                                    Text { text: "Размер комнаты: " + Math.round(root.effectReverb * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectReverb
                                        onMoved: root.effectReverb = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ЭХО
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Эхо"
                                    Text { text: "Задержка: " + Math.round(root.effectEcho * 300) + " мс"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectEcho
                                        onMoved: root.effectEcho = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }


                                // ИНВЕРСИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Инверсия"
                                    Text { text: "Инвертировать цвета"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    CheckBox {
                                        text: "Включить"; checked: root.effectInvert
                                        onToggled: root.effectInvert = checked
                                        contentItem: Text { text: parent.text; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter }
                                        indicator: Rectangle { width: 20; height: 20; radius: 3; color: parent.checked ? Theme.rubyPrimary : Theme.backgroundDark; border.color: Theme.rubyPrimary; border.width: 2; Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 16; font.bold: true; visible: parent.parent.checked } }
                                    }
                                }

                                // ПОСТЕРИЗАЦИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Постеризация"
                                    Text { text: "Уровней: " + (root.effectPosterize < 2 ? "выкл" : Math.round(root.effectPosterize).toString()); color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0; to: 8; stepSize: 1; value: root.effectPosterize
                                        onMoved: root.effectPosterize = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ПИКСЕЛИЗАЦИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Пикселизация"
                                    Text { text: "Размер блока: " + (root.effectPixelate < 2 ? "выкл" : Math.round(root.effectPixelate) + " px"); color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0; to: 64; stepSize: 1; value: root.effectPixelate
                                        onMoved: root.effectPixelate = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ТЕМПЕРАТУРА
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Температура"
                                    Text {
                                        text: root.effectTemperature > 0 ? "🔥 Тёплый: +" + root.effectTemperature.toFixed(2)
                                            : root.effectTemperature < 0 ? "❄️ Холодный: " + root.effectTemperature.toFixed(2)
                                            : "Нейтральный"
                                        color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                    }
                                    Slider {
                                        width: parent.width; from: -1.0; to: 1.0; stepSize: 0.05; value: root.effectTemperature
                                        onMoved: root.effectTemperature = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2
                                            gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0.0; color: "#4488FF" } GradientStop { position: 0.5; color: Theme.backgroundDark } GradientStop { position: 1.0; color: "#FF8844" } }
                                            Rectangle { x: parent.parent.visualPosition * parent.width - width/2; width: 2; height: parent.height; color: "white"; radius: 1 }
                                        }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ТИНТ (цветовой оттенок)
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Тинт"
                                    Text { text: "Цвет: " + Math.round(root.effectTintHue) + "°"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0; to: 360; stepSize: 1; value: root.effectTintHue
                                        onMoved: root.effectTintHue = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2
                                            gradient: Gradient { orientation: Gradient.Horizontal
                                                GradientStop { position: 0.0;   color: "#FF0000" }
                                                GradientStop { position: 0.166; color: "#FFFF00" }
                                                GradientStop { position: 0.333; color: "#00FF00" }
                                                GradientStop { position: 0.5;   color: "#00FFFF" }
                                                GradientStop { position: 0.666; color: "#0000FF" }
                                                GradientStop { position: 0.833; color: "#FF00FF" }
                                                GradientStop { position: 1.0;   color: "#FF0000" }
                                            }
                                        }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                    Text { text: "Сила: " + Math.round(root.effectTintStr * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectTintStr
                                        onMoved: root.effectTintStr = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // МОНО
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Моно"
                                    Text { text: "Конвертировать в моно"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Text { text: "Оба канала станут одинаковыми"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    CheckBox {
                                        text: "Включить"; checked: root.effectMono
                                        onToggled: root.effectMono = checked
                                        contentItem: Text { text: parent.text; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter }
                                        indicator: Rectangle { width: 20; height: 20; radius: 3; color: parent.checked ? Theme.rubyPrimary : Theme.backgroundDark; border.color: Theme.rubyPrimary; border.width: 2; Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 16; font.bold: true; visible: parent.parent.checked } }
                                    }
                                }

                                // РАСШИРЕНИЕ СТЕРЕО
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Стерео"
                                    Text { text: "Ширина: " + Math.round(root.effectStereoWiden * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Text { text: "0% = моно, 100% = широкое стерео (Haas-эффект)"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectStereoWiden
                                        onMoved: root.effectStereoWiden = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ПИТЧ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Питч"
                                    Text { text: "Высота тона: " + (root.effectPitch >= 0 ? "+" : "") + root.effectPitch.toFixed(1) + " пт"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: -12; to: 12; stepSize: 0.5; value: root.effectPitch
                                        onMoved: root.effectPitch = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // НОРМАЛИЗАЦИЯ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Нормализация"
                                    Text { text: "Целевой уровень: " + Math.round(root.effectNormalize * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Text { text: "Усиливает тихий звук до заданного пика"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.WordWrap }
                                    Slider {
                                        width: parent.width; from: 0.1; to: 1.0; stepSize: 0.05; value: root.effectNormalize
                                        onMoved: root.effectNormalize = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ФЕЙД-ИН
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Фейд-ин"
                                    Text { text: "Нарастание: " + Math.round(root.effectFadeIn * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectFadeIn
                                        onMoved: root.effectFadeIn = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                // ФЕЙД-АУТ
                                Column {
                                    Layout.fillWidth: true; spacing: Theme.spacing
                                    visible: root.selectedEffect === "Фейд-аут"
                                    Text { text: "Затухание: " + Math.round(root.effectFadeOut * 100) + "%"; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                                    Slider {
                                        width: parent.width; from: 0.0; to: 1.0; stepSize: 0.05; value: root.effectFadeOut
                                        onMoved: root.effectFadeOut = value
                                        background: Rectangle { x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2; width: parent.availableWidth; height: 4; radius: 2; color: Theme.backgroundDark; Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; color: Theme.rubyPrimary; radius: 2 } }
                                        handle: Rectangle { x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width/2; y: parent.topPadding + parent.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary }
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                // Кнопки Применить / Сбросить
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    visible: root.selectedEffect !== "" && root.selectedClipId >= 0

                                    Button {
                                        Layout.fillWidth: true; Layout.preferredHeight: 32
                                        text: "Применить"
                                        onClicked: {
                                            if (!cppTimeline || root.selectedClipId < 0) return
                                            var id = root.selectedClipId; var e = root.selectedEffect
                                            if      (e === "Яркость")      cppTimeline.applyEffect(id, "brightness", root.effectBrightness)
                                            else if (e === "Контраст")     cppTimeline.applyEffect(id, "contrast",   root.effectContrast)
                                            else if (e === "Насыщенность") { cppTimeline.applyEffect(id, "saturation", root.effectSaturation); cppTimeline.applyEffect(id, "grayscale", root.effectGrayscale ? 1.0 : 0.0) }
                                            else if (e === "Размытие")     cppTimeline.applyEffect(id, "blur",      root.effectBlur)
                                            else if (e === "Резкость")     cppTimeline.applyEffect(id, "sharpness", root.effectSharpness)
                                            else if (e === "Громкость")    cppTimeline.applyEffect(id, "volume",    root.effectVolume)
                                            else if (e === "Оттенок")     cppTimeline.applyEffect(id, "hue",       root.effectHue)
                                            else if (e === "Сепия")       cppTimeline.applyEffect(id, "sepia",     root.effectSepia)
                                            else if (e === "Виньетка")    cppTimeline.applyEffect(id, "vignette",  root.effectVignette)
                                            else if (e === "Реверберация")cppTimeline.applyEffect(id, "reverb",   root.effectReverb)
                                            else if (e === "Эхо")         cppTimeline.applyEffect(id, "echo",      root.effectEcho)
                                            else if (e === "Инверсия")    cppTimeline.applyEffect(id, "invert",    root.effectInvert ? 1.0 : 0.0)
                                            else if (e === "Постеризация")cppTimeline.applyEffect(id, "posterize", root.effectPosterize)
                                            else if (e === "Пикселизация")cppTimeline.applyEffect(id, "pixelate",  root.effectPixelate)
                                            else if (e === "Температура") cppTimeline.applyEffect(id, "temperature",root.effectTemperature)
                                            else if (e === "Тинт") {
                                                cppTimeline.applyEffect(id, "tint_hue",      root.effectTintHue)
                                                cppTimeline.applyEffect(id, "tint_strength", root.effectTintStr)
                                            }
                                            else if (e === "Моно")        cppTimeline.applyEffect(id, "mono",         root.effectMono ? 1.0 : 0.0)
                                            else if (e === "Стерео")      cppTimeline.applyEffect(id, "stereo_widen", root.effectStereoWiden)
                                            else if (e === "Питч")        cppTimeline.applyEffect(id, "pitch",        root.effectPitch)
                                            else if (e === "Нормализация")cppTimeline.applyEffect(id, "normalize",   root.effectNormalize)
                                            else if (e === "Фейд-ин")     cppTimeline.applyEffect(id, "fade_in",      root.effectFadeIn)
                                            else if (e === "Фейд-аут")    cppTimeline.applyEffect(id, "fade_out",     root.effectFadeOut)
                                        }
                                        contentItem: Text { text: parent.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary); radius: Theme.borderRadius }
                                    }

                                    Button {
                                        Layout.preferredWidth: 76; Layout.preferredHeight: 32
                                        text: "Сбросить"
                                        onClicked: {
                                            if (!cppTimeline || root.selectedClipId < 0) return
                                            var id = root.selectedClipId
                                            cppTimeline.removeEffect(id, "brightness"); cppTimeline.removeEffect(id, "contrast")
                                            cppTimeline.removeEffect(id, "saturation"); cppTimeline.removeEffect(id, "grayscale")
                                            cppTimeline.removeEffect(id, "blur");       cppTimeline.removeEffect(id, "sharpness")
                                            cppTimeline.removeEffect(id, "volume")
                                            cppTimeline.removeEffect(id, "hue");       cppTimeline.removeEffect(id, "sepia")
                                            cppTimeline.removeEffect(id, "vignette");  cppTimeline.removeEffect(id, "reverb")
                                            cppTimeline.removeEffect(id, "echo")
                                            cppTimeline.removeEffect(id, "invert");       cppTimeline.removeEffect(id, "posterize")
                                            cppTimeline.removeEffect(id, "pixelate");     cppTimeline.removeEffect(id, "temperature")
                                            cppTimeline.removeEffect(id, "tint_hue");     cppTimeline.removeEffect(id, "tint_strength")
                                            cppTimeline.removeEffect(id, "mono");         cppTimeline.removeEffect(id, "stereo_widen")
                                            cppTimeline.removeEffect(id, "pitch");        cppTimeline.removeEffect(id, "normalize")
                                            cppTimeline.removeEffect(id, "fade_in");      cppTimeline.removeEffect(id, "fade_out")
                                            root.loadEffectsFromClip(root.selectedClipId)
                                        }
                                        contentItem: Text { text: parent.text; color: "#EF5350"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.down ? Qt.rgba(0.94,0.33,0.31,0.3) : (parent.hovered ? Qt.rgba(0.94,0.33,0.31,0.15) : "transparent"); border.color: "#EF5350"; border.width: 1; radius: Theme.borderRadius }
                                    }
                                }
                            }
                        }
                    }
                }

                // Список эффектов — нижние 2/3
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"

                    ScrollView {
                        anchors.fill: parent
                        clip: true

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6; radius: 3
                                color: Theme.rubyPrimary
                                opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                            }
                        }

                        Column {
                            width: parent.parent.width - 20
                            spacing: 0

                            EffectCategory {
                                title: "Видео: Цвет и тон"
                                effects: [
                                    { "name": "Яркость",      "icon": "☀️" },
                                    { "name": "Контраст",     "icon": "🔆" },
                                    { "name": "Насыщенность", "icon": "🎨" },
                                    { "name": "Оттенок",      "icon": "🌈" },
                                    { "name": "Температура",  "icon": "🌡️" },
                                    { "name": "Тинт",         "icon": "🖌️" },
                                    { "name": "Сепия",        "icon": "🟤" }
                                ]
                            }

                            EffectCategory {
                                title: "Видео: Стилизация"
                                effects: [
                                    { "name": "Инверсия",     "icon": "⬛" },
                                    { "name": "Постеризация", "icon": "🎭" },
                                    { "name": "Пикселизация", "icon": "⊞"  }
                                ]
                            }

                            EffectCategory {
                                title: "Видео: Резкость и фокус"
                                effects: [
                                    { "name": "Размытие",     "icon": "◎"  },
                                    { "name": "Резкость",     "icon": "⬥"  },
                                    { "name": "Виньетка",     "icon": "🔳" }
                                ]
                            }

                            EffectCategory {
                                title: "Аудио: Динамика"
                                effects: [
                                    { "name": "Громкость",     "icon": "🔊" },
                                    { "name": "Нормализация",  "icon": "📊" },
                                    { "name": "Фейд-ин",       "icon": "📈" },
                                    { "name": "Фейд-аут",      "icon": "📉" }
                                ]
                            }

                            EffectCategory {
                                title: "Аудио: Пространство"
                                effects: [
                                    { "name": "Реверберация",  "icon": "〰️" },
                                    { "name": "Эхо",           "icon": "↩️" },
                                    { "name": "Стерео",        "icon": "🎧" },
                                    { "name": "Моно",          "icon": "🔈" }
                                ]
                            }

                            EffectCategory {
                                title: "Аудио: Тон"
                                effects: [
                                    { "name": "Питч",          "icon": "🎵" }
                                ]
                            }
                        }
                    }
                }
            }
        }

        // ══════════ РЕЖИМ ЭКСПОРТА ══════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentMode === 1

            ExportPanel {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                onExportClicked: (res, fmt) => root.exportRequested(res, fmt)
            }
        }
    }

    component ExportPanel: ColumnLayout {
        spacing: Theme.spacingLarge

        signal exportClicked(string resolution, string format)

        Text {
            text: "ЭКСПОРТ ВИДЕО"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
        }

        SettingRow {
            label: "Разрешение"
            ComboBox {
                id: resolutionComboExport
                implicitWidth: 150
                model: ["1920×1080", "1280×720", "3840×2160", "2560×1440"]
                currentIndex: 0
                contentItem: Text { text: parent.displayText; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; verticalAlignment: Text.AlignVCenter; leftPadding: Theme.spacing }
                background: Rectangle { color: parent.down ? Theme.buttonPressed : (parent.hovered ? Theme.buttonHover : Theme.buttonBackground); radius: Theme.borderRadius; border.color: Theme.rubyPrimary; border.width: 1 }
            }
        }

        SettingRow {
            label: "Формат"
            ComboBox {
                id: formatComboExport
                implicitWidth: 150
                model: ["MP4", "AVI", "MOV", "MKV", "WebM"]
                currentIndex: 0
                contentItem: Text { text: parent.displayText; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; verticalAlignment: Text.AlignVCenter; leftPadding: Theme.spacing }
                background: Rectangle { color: parent.down ? Theme.buttonPressed : (parent.hovered ? Theme.buttonHover : Theme.buttonBackground); radius: Theme.borderRadius; border.color: Theme.rubyPrimary; border.width: 1 }
            }
        }

        Button {
            text: "СОХРАНИТЬ ВИДЕО"
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            contentItem: Text { text: parent.text; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle {
                color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary)
                radius: Theme.borderRadius
                Behavior on color { ColorAnimation { duration: Theme.animationDuration } }
            }
            onClicked: exportClicked(resolutionComboExport.currentText, formatComboExport.currentText)
        }

        Item { Layout.fillHeight: true }
    }

    component EffectCategory: Column {
        property string title: ""
        property var effects: []
        property bool collapsed: false

        width: parent.width
        spacing: 0

        Rectangle {
            width: parent.width
            height: 35
            color: headerMouseArea.containsMouse ? Theme.hoverColor : Theme.backgroundDark
            radius: Theme.borderRadius

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing
                anchors.rightMargin: Theme.spacing
                spacing: Theme.spacing
                Text { text: collapsed ? "▶" : "▼"; color: Theme.rubyPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                Text { text: title; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; font.bold: true; Layout.fillWidth: true }
            }

            MouseArea {
                id: headerMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: parent.parent.collapsed = !parent.parent.collapsed
            }
        }

        Column {
            width: parent.width
            spacing: Theme.spacingSmall
            visible: !parent.collapsed
            height: visible ? implicitHeight : 0
            Behavior on height { NumberAnimation { duration: Theme.animationDuration } }

            Repeater {
                model: effects

                Rectangle {
                    width: parent.width
                    height: 36
                    color: effectMouseArea.containsMouse ? Theme.hoverColor : "transparent"
                    radius: Theme.borderRadius

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingLarge * 2
                        anchors.rightMargin: Theme.spacing
                        spacing: Theme.spacing
                        Text { text: modelData.icon; color: Theme.rubyLight; font.pixelSize: 16 }
                        Text { text: modelData.name; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; Layout.fillWidth: true }
                    }

                    MouseArea {
                        id: effectMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.selectedEffect = modelData.name
                            console.log("Выбран эффект:", modelData.name)
                        }
                    }
                }
            }
        }
    }

    component SettingRow: RowLayout {
        property string label: ""
        Layout.fillWidth: true
        spacing: Theme.spacing
        Text { text: label; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; Layout.fillWidth: true }
    }
}
