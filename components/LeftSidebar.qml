/**
 * LeftSidebar
 * -----------
 * The app's left panel with two independent modes (switched via
 * currentMode, driven from outside by ModeSwitcher):
 *   0 — Effects mode: three main tabs (Video/Audio/Transitions); Video
 *       and Audio each have nested sub-tabs. Editor effects are held as
 *       a "draft" in property fields (effectBrightness, etc.) — moving a
 *       slider immediately updates the live preview via a binding to
 *       videoPlayer.effectXxx, but changes are NOT written to C++/the
 *       project until the user clicks "Apply" (applyCurrentEffect). This
 *       lets you drag a slider and see the result live without creating
 *       extra history/model entries on every micro-change.
 *   1 — Export mode: a simple export-settings form (ExportPanel).
 *
 * All reusable visual blocks are extracted into inline components at the
 * end of the file (FxSlider, FxCheck, FxCategory, TransitionCard,
 * ExportPanel, SettingRow) — the UI markup earlier in the file is almost
 * entirely repeated combinations of these components, one set per effect.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground

    property int currentMode: 0
    property var videoPlayer: null
    property string selectedEffect: "" // name of the effect selected in the category list (used by applyCurrentEffect)

    signal exportRequested(string resolution, string format)

    property int selectedClipId: -1

    // Video effects — "draft" values for the live-preview sliders;
    // the real values are applied to C++ separately, via applyCurrentEffect()
    property real effectBrightness: 0.0
    property real effectContrast: 1.0
    property real effectSaturation: 1.0
    property bool effectGrayscale: false
    property real effectBlur: 0.0
    property real effectSharpness: 0.0
    property real effectHue: 0.0
    property real effectSepia: 0.0
    property real effectVignette: 0.0
    property bool effectInvert: false
    property real effectPosterize: 0.0
    property real effectPixelate: 0.0
    property real effectTemperature: 0.0
    property real effectTintHue: 0.0
    property real effectTintStr: 0.0
    property real effectGrain: 0.0
    property real effectAutoEnhance: 0.5 // сила авто-улучшения 0..1
    property bool effectChromaKey: false
    property real effectChromaThreshold: 0.35
    property real effectChromaSmoothness: 0.1

    // Аудио эффекты
    property real effectVolume: 1.0
    property real effectReverb: 0.0
    property real effectEcho: 0.0
    property bool effectMono: false
    property real effectStereoWiden: 0.0
    property real effectPitch: 0.0
    property real effectNormalize: 0.0
    property real effectFadeIn: 0.0
    property real effectFadeOut: 0.0

    // Переходы
    property string transitionIn: "none"
    property string transitionOut: "none"
    property real transitionDur: 0.5

    // Навигация
    property int mainTab: 0 // 0=Видео 1=Аудио 2=Переходы
    property int videoSubTab: 0 // 0=Цвет 1=Стилизация 2=Фокус
    property int audioSubTab: 0 // 0=Динамика 1=Пространство 2=Тон

    // Loads the currently selected clip's effect values from C++ into
    // the local properties (the slider draft). When clipId < 0 (nothing
    // selected) — resets everything to the default values. Called
    // automatically whenever selectedClipId changes (see the handler below)
    function loadEffectsFromClip(clipId) {
        if (clipId < 0 || !cppTimeline) {
            effectBrightness = 0.0
            effectContrast = 1.0
            effectSaturation = 1.0
            effectGrayscale = false
            effectBlur = 0.0
            effectSharpness = 0.0
            effectHue = 0.0
            effectSepia = 0.0
            effectVignette = 0.0
            effectInvert = false
            effectPosterize = 0.0
            effectPixelate = 0.0
            effectTemperature = 0.0
            effectTintHue = 0.0
            effectTintStr = 0.0
            effectGrain = 0.0
            effectChromaKey = false
            effectChromaThreshold = 0.35
            effectChromaSmoothness = 0.1
            effectVolume = 1.0
            effectReverb = 0.0
            effectEcho = 0.0
            effectMono = false
            effectStereoWiden = 0.0
            effectPitch = 0.0
            effectNormalize = 0.0
            effectFadeIn = 0.0
            effectFadeOut = 0.0
            transitionIn = "none"
            transitionOut = "none"
            transitionDur = 0.5
            return
        }
        var fx = cppTimeline.getClipEffects(clipId)
        effectBrightness = fx["brightness"] !== undefined ? fx["brightness"] : 0.0
        effectContrast = fx["contrast"] !== undefined ? fx["contrast"] : 1.0
        effectSaturation = fx["saturation"] !== undefined ? fx["saturation"] : 1.0
        effectGrayscale = fx["grayscale"] !== undefined ? fx["grayscale"] > 0.5 : false
        effectBlur = fx["blur"] !== undefined ? fx["blur"] : 0.0
        effectSharpness = fx["sharpness"] !== undefined ? fx["sharpness"] : 0.0
        effectHue = fx["hue"] !== undefined ? fx["hue"] : 0.0
        effectSepia = fx["sepia"] !== undefined ? fx["sepia"] : 0.0
        effectVignette = fx["vignette"] !== undefined ? fx["vignette"] : 0.0
        effectInvert = fx["invert"] !== undefined ? fx["invert"] > 0.5 : false
        effectPosterize = fx["posterize"] !== undefined ? fx["posterize"] : 0.0
        effectPixelate = fx["pixelate"] !== undefined ? fx["pixelate"] : 0.0
        effectTemperature = fx["temperature"] !== undefined ? fx["temperature"] : 0.0
        effectTintHue = fx["tint_hue"] !== undefined ? fx["tint_hue"] : 0.0
        effectTintStr = fx["tint_strength"] !== undefined ? fx["tint_strength"] : 0.0
        effectGrain = fx["grain"] !== undefined ? fx["grain"] : 0.0
        effectAutoEnhance = fx["auto_enhance"] !== undefined ? fx["auto_enhance"] : 0.5
        effectChromaKey = fx["chroma_key"] !== undefined ? fx["chroma_key"] > 0.5 : false
        effectChromaThreshold = fx["chroma_threshold"] !== undefined ? fx["chroma_threshold"] : 0.35
        effectChromaSmoothness = fx["chroma_smoothness"]
                !== undefined ? fx["chroma_smoothness"] : 0.1
        effectVolume = fx["volume"] !== undefined ? fx["volume"] : 1.0
        effectReverb = fx["reverb"] !== undefined ? fx["reverb"] : 0.0
        effectEcho = fx["echo"] !== undefined ? fx["echo"] : 0.0
        effectMono = fx["mono"] !== undefined ? fx["mono"] > 0.5 : false
        effectStereoWiden = fx["stereo_widen"] !== undefined ? fx["stereo_widen"] : 0.0
        effectPitch = fx["pitch"] !== undefined ? fx["pitch"] : 0.0
        effectNormalize = fx["normalize"] !== undefined ? fx["normalize"] : 0.0
        effectFadeIn = fx["fade_in"] !== undefined ? fx["fade_in"] : 0.0
        effectFadeOut = fx["fade_out"] !== undefined ? fx["fade_out"] : 0.0
        if (cppTimeline.getClipTransitions) {
            var tr = cppTimeline.getClipTransitions(clipId)
            transitionIn = tr["in"] !== undefined ? tr["in"] : "none"
            transitionOut = tr["out"] !== undefined ? tr["out"] : "none"
            transitionDur = tr["duration"] !== undefined ? tr["duration"] : 0.5
        }
    }

    onSelectedClipIdChanged: loadEffectsFromClip(selectedClipId)

    // Applies the currently selected effect (root.selectedEffect) to the
    // clip, pushing its "draft" value from the property into C++. The
    // mapping "Russian display name" → "C++ effect key" is implemented as
    // an if/else chain over the human-readable Russian labels (the same
    // ones shown in FxCategory) — not the most flexible approach, but
    // direct and easy to read. Some effects write two or three keys at
    // once (e.g. "Насыщенность" [Saturation] sets both saturation AND
    // grayscale, "Тинт" [Tint] sets tint_hue and tint_strength) — that's
    // how the corresponding parameters interact in the C++ renderer
    function applyCurrentEffect() {
        if (!cppTimeline || root.selectedClipId < 0
                || root.selectedEffect === "")
            return
        var id = root.selectedClipId
        var e = root.selectedEffect
        if (e === "Яркость")
            cppTimeline.applyEffect(id, "brightness", root.effectBrightness)
        else if (e === "Контраст")
            cppTimeline.applyEffect(id, "contrast", root.effectContrast)
        else if (e === "Насыщенность") {
            cppTimeline.applyEffect(id, "saturation", root.effectSaturation)
            cppTimeline.applyEffect(id, "grayscale",
                                    root.effectGrayscale ? 1.0 : 0.0)
        } else if (e === "Оттенок")
            cppTimeline.applyEffect(id, "hue", root.effectHue)
        else if (e === "Температура")
            cppTimeline.applyEffect(id, "temperature", root.effectTemperature)
        else if (e === "Тинт") {
            cppTimeline.applyEffect(id, "tint_hue", root.effectTintHue)
            cppTimeline.applyEffect(id, "tint_strength", root.effectTintStr)
        } else if (e === "Сепия")
            cppTimeline.applyEffect(id, "sepia", root.effectSepia)
        else if (e === "Ч/Б")
            cppTimeline.applyEffect(id, "grayscale", 1.0)
        else if (e === "Инверсия")
            cppTimeline.applyEffect(id, "invert", root.effectInvert ? 1.0 : 0.0)
        else if (e === "Постеризация")
            cppTimeline.applyEffect(id, "posterize", root.effectPosterize)
        else if (e === "Пикселизация")
            cppTimeline.applyEffect(id, "pixelate", root.effectPixelate)
        else if (e === "Зернистость")
            cppTimeline.applyEffect(id, "grain", root.effectGrain)
        else if (e === "Хромакей") {
            cppTimeline.applyEffect(id, "chroma_key",
                                    root.effectChromaKey ? 1.0 : 0.0)
            cppTimeline.applyEffect(id, "chroma_threshold",
                                    root.effectChromaThreshold)
            cppTimeline.applyEffect(id, "chroma_smoothness",
                                    root.effectChromaSmoothness)
        } else if (e === "Размытие")
            cppTimeline.applyEffect(id, "blur", root.effectBlur)
        else if (e === "Резкость")
            cppTimeline.applyEffect(id, "sharpness", root.effectSharpness)
        else if (e === "Виньетка")
            cppTimeline.applyEffect(id, "vignette", root.effectVignette)
        else if (e === "Громкость")
            cppTimeline.applyEffect(id, "volume", root.effectVolume)
        else if (e === "Нормализация")
            cppTimeline.applyEffect(id, "normalize", root.effectNormalize)
        else if (e === "Фейд-ин")
            cppTimeline.applyEffect(id, "fade_in", root.effectFadeIn)
        else if (e === "Фейд-аут")
            cppTimeline.applyEffect(id, "fade_out", root.effectFadeOut)
        else if (e === "Реверберация")
            cppTimeline.applyEffect(id, "reverb", root.effectReverb)
        else if (e === "Эхо")
            cppTimeline.applyEffect(id, "echo", root.effectEcho)
        else if (e === "Моно")
            cppTimeline.applyEffect(id, "mono", root.effectMono ? 1.0 : 0.0)
        else if (e === "Стерео")
            cppTimeline.applyEffect(id, "stereo_widen", root.effectStereoWiden)
        else if (e === "Питч")
            cppTimeline.applyEffect(id, "pitch", root.effectPitch)
        else if (e === "Авто-улучшение")
            cppTimeline.applyEffect(id, "auto_enhance", root.effectAutoEnhance)
    }

    // Resets all effects on the current clip: a hard-coded list of keys
    // (matches the list in ClipEffectsDialog.resetAll) gets removed from
    // C++, then the draft properties are re-read from the now-empty state
    function resetAllEffects() {
        if (!cppTimeline || root.selectedClipId < 0)
            return
        var id = root.selectedClipId
        var keys = ["brightness", "contrast", "saturation", "grayscale", "blur", "sharpness", "hue", "sepia", "vignette", "invert", "posterize", "pixelate", "temperature", "tint_hue", "tint_strength", "grain", "chroma_key", "chroma_threshold", "chroma_smoothness", "volume", "reverb", "echo", "mono", "stereo_widen", "pitch", "normalize", "fade_in", "fade_out", "auto_enhance"]
        for (var i = 0; i < keys.length; ++i)
            cppTimeline.removeEffect(id, keys[i])
        loadEffectsFromClip(id)
    }

    // Правая граница-декор
    Rectangle {
        anchors.right: parent.right
        width: 2
        height: parent.height
        gradient: Gradient {
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // РЕЖИМ ЭФФЕКТОВ
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentMode === 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: 6

                // Main tabs: Video / Audio / Transitions.
                // Switching tabs resets selectedEffect — so an effect
                // from another category doesn't stay selected when you come back
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: Theme.backgroundDark
                    radius: Theme.borderRadius
                    RowLayout {
                        anchors {
                            fill: parent
                            margins: 3
                        }
                        spacing: 3
                        Repeater {
                            model: [{
                                    "label": "🎬  Видео",
                                    "idx": 0
                                }, {
                                    "label": "🎵  Аудио",
                                    "idx": 1
                                }, {
                                    "label": "✨  Переходы",
                                    "idx": 2
                                }]
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Theme.borderRadius - 1
                                color: root.mainTab === modelData.idx ? Theme.rubyPrimary : (mtMa.containsMouse ? Qt.rgba(0.8, 0.1, 0.2, 0.2) : "transparent")
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: root.mainTab
                                           === modelData.idx ? "white" : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.bold: root.mainTab === modelData.idx
                                }
                                MouseArea {
                                    id: mtMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        root.mainTab = modelData.idx
                                        root.selectedEffect = ""
                                    }
                                }
                            }
                        }
                    }
                }

                // Подвкладки
                // Sub-tabs: the set of items depends on mainTab (Video
                // and Audio have different sub-categories); hidden
                // entirely on the "Transitions" tab (mainTab === 2), since
                // that tab has its own separate layout below with no sub-tabs
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "transparent"
                    visible: root.mainTab !== 2

                    RowLayout {
                        anchors.fill: parent
                        spacing: 4

                        Repeater {
                            model: root.mainTab === 0 ? [{
                                                             "label": "Цвет и тон",
                                                             "idx": 0
                                                         }, {
                                                             "label": "Стилизация",
                                                             "idx": 1
                                                         }, {
                                                             "label": "Фокус",
                                                             "idx": 2
                                                         }] : [{
                                                                   "label": "Динамика",
                                                                   "idx": 0
                                                               }, {
                                                                   "label": "Пространство",
                                                                   "idx": 1
                                                               }, {
                                                                   "label": "Тон",
                                                                   "idx": 2
                                                               }]

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 4
                                property bool active: root.mainTab === 0 ? root.videoSubTab === modelData.idx : root.audioSubTab === modelData.idx
                                color: active ? Qt.rgba(
                                                    0.8, 0.1, 0.2,
                                                    0.3) : (stMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                                border.color: active ? Theme.rubyPrimary : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 100
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: active ? Theme.rubyLight : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    font.bold: active
                                }
                                MouseArea {
                                    id: stMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        if (root.mainTab === 0)
                                            root.videoSubTab = modelData.idx
                                        else
                                            root.audioSubTab = modelData.idx
                                        root.selectedEffect = ""
                                    }
                                }
                            }
                        }
                    }
                }

                // Панель параметров
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: paramsCol.implicitHeight + 16
                    visible: root.selectedEffect !== "" && root.mainTab !== 2
                    color: Theme.backgroundDark
                    radius: Theme.borderRadius
                    border.color: Theme.rubyPrimary
                    border.width: 1

                    ColumnLayout {
                        id: paramsCol
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 8
                        }
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: root.selectedEffect
                                color: Theme.rubyLight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 20
                                height: 20
                                radius: 10
                                color: xMa.containsMouse ? Qt.rgba(
                                                               1, 0, 0,
                                                               0.3) : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: Theme.textSecondary
                                    font.pixelSize: 10
                                }
                                MouseArea {
                                    id: xMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: root.selectedEffect = ""
                                }
                            }
                        }

                        Text {
                            visible: root.selectedClipId < 0
                            text: "⚠️ Выберите клип на таймлайне"
                            color: "#FFA726"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.dividerColor
                        }

                        // Контролы эффектов

                        // ЯРКОСТЬ
                        FxSlider {
                            visible: root.selectedEffect === "Яркость"
                            label: "Яркость"
                            valText: (root.effectBrightness >= 0 ? "+" : "")
                                     + root.effectBrightness.toFixed(2)
                            from: -1
                            to: 1
                            step: .05
                            val: root.effectBrightness
                            onMv: function (v) {
                                root.effectBrightness = v
                            }
                        }

                        // КОНТРАСТ
                        FxSlider {
                            visible: root.selectedEffect === "Контраст"
                            label: "Контраст"
                            valText: root.effectContrast.toFixed(2)
                            from: 0
                            to: 3
                            step: .05
                            val: root.effectContrast
                            onMv: function (v) {
                                root.effectContrast = v
                            }
                        }

                        // НАСЫЩЕННОСТЬ
                        Column {
                            visible: root.selectedEffect === "Насыщенность"
                            spacing: 6
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxSlider {
                                label: "Насыщенность"
                                valText: root.effectSaturation.toFixed(2) + "x"
                                from: 0
                                to: 2
                                step: .05
                                val: root.effectSaturation
                                onMv: function (v) {
                                    root.effectSaturation = v
                                }
                            }
                            FxCheck {
                                label: "Обесцветить (Ч/Б)"
                                chk: root.effectGrayscale
                                onTg: function () {
                                    root.effectGrayscale = !root.effectGrayscale
                                }
                            }
                        }

                        // ОТТЕНОК
                        FxSlider {
                            visible: root.selectedEffect === "Оттенок"
                            label: "Сдвиг оттенка"
                            valText: root.effectHue.toFixed(0) + "°"
                            from: -180
                            to: 180
                            step: 1
                            val: root.effectHue
                            rainbow: true
                            onMv: function (v) {
                                root.effectHue = v
                            }
                        }

                        // ТЕМПЕРАТУРА
                        FxSlider {
                            visible: root.selectedEffect === "Температура"
                            label: (root.effectTemperature > 0 ? "🔥 Тёплый" : "❄️ Холодный")
                            valText: (root.effectTemperature >= 0 ? "+" : "")
                                     + root.effectTemperature.toFixed(2)
                            from: -1
                            to: 1
                            step: .05
                            val: root.effectTemperature
                            warm: true
                            onMv: function (v) {
                                root.effectTemperature = v
                            }
                        }

                        // ТИНТ
                        Column {
                            visible: root.selectedEffect === "Тинт"
                            spacing: 6
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxSlider {
                                label: "Цвет"
                                valText: root.effectTintHue.toFixed(0) + "°"
                                from: 0
                                to: 360
                                step: 1
                                val: root.effectTintHue
                                rainbow: true
                                onMv: function (v) {
                                    root.effectTintHue = v
                                }
                            }
                            FxSlider {
                                label: "Сила"
                                valText: Math.round(
                                             root.effectTintStr * 100) + "%"
                                from: 0
                                to: 1
                                step: .05
                                val: root.effectTintStr
                                onMv: function (v) {
                                    root.effectTintStr = v
                                }
                            }
                        }

                        // СЕПИЯ
                        FxSlider {
                            visible: root.selectedEffect === "Сепия"
                            label: "Интенсивность"
                            valText: Math.round(root.effectSepia * 100) + "%"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectSepia
                            onMv: function (v) {
                                root.effectSepia = v
                            }
                        }

                        // Ч/Б
                        Text {
                            visible: root.selectedEffect === "Ч/Б"
                            text: "Преобразует все цвета в оттенки серого"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        // ИНВЕРСИЯ
                        FxCheck {
                            visible: root.selectedEffect === "Инверсия"
                            label: "Инвертировать цвета"
                            chk: root.effectInvert
                            onTg: function () {
                                root.effectInvert = !root.effectInvert
                            }
                        }

                        // ПОСТЕРИЗАЦИЯ
                        FxSlider {
                            visible: root.selectedEffect === "Постеризация"
                            label: "Уровней цвета"
                            valText: root.effectPosterize < 2 ? "выкл" : Math.round(
                                                                    root.effectPosterize).toString()
                            from: 0
                            to: 8
                            step: 1
                            val: root.effectPosterize
                            onMv: function (v) {
                                root.effectPosterize = v
                            }
                        }

                        // ПИКСЕЛИЗАЦИЯ
                        FxSlider {
                            visible: root.selectedEffect === "Пикселизация"
                            label: "Размер блока"
                            valText: root.effectPixelate < 2 ? "выкл" : Math.round(
                                                                   root.effectPixelate) + " px"
                            from: 0
                            to: 64
                            step: 1
                            val: root.effectPixelate
                            onMv: function (v) {
                                root.effectPixelate = v
                            }
                        }

                        // ЗЕРНИСТОСТЬ
                        FxSlider {
                            visible: root.selectedEffect === "Зернистость"
                            label: "Интенсивность"
                            valText: Math.round(root.effectGrain * 100) + "%"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectGrain
                            onMv: function (v) {
                                root.effectGrain = v
                            }
                        }

                        // ХРОМАКЕЙ
                        Column {
                            visible: root.selectedEffect === "Хромакей"
                            spacing: 6
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxCheck {
                                label: "Убрать зелёный фон"
                                chk: root.effectChromaKey
                                onTg: function () {
                                    root.effectChromaKey = !root.effectChromaKey
                                }
                            }
                            FxSlider {
                                label: "Порог чувствительности"
                                valText: Math.round(
                                             root.effectChromaThreshold * 100) + "%"
                                from: .05
                                to: .8
                                step: .01
                                val: root.effectChromaThreshold
                                onMv: function (v) {
                                    root.effectChromaThreshold = v
                                }
                            }
                            FxSlider {
                                label: "Размытие краёв"
                                valText: Math.round(
                                             root.effectChromaSmoothness * 100) + "%"
                                from: 0
                                to: .3
                                step: .01
                                val: root.effectChromaSmoothness
                                onMv: function (v) {
                                    root.effectChromaSmoothness = v
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: chromaHint.implicitHeight + 10
                                color: Qt.rgba(0, .5, .1, .15)
                                radius: 4
                                border.color: Qt.rgba(0, .8, .2, .4)
                                border.width: 1
                                Text {
                                    id: chromaHint
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    text: "💡 Дорожка 1 — видео с зелёным фоном\nДорожка 2 — фоновое видео или изображение"
                                    color: "#80e080"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // АВТО-УЛУЧШЕНИЕ
                        Column {
                            visible: root.selectedEffect === "Авто-улучшение"
                            spacing: 6
                            width: parent.width
                            height: visible ? implicitHeight : 0

                            FxSlider {
                                label: "Сила улучшения"
                                valText: Math.round(
                                             root.effectAutoEnhance * 100) + "%"
                                from: 0.0
                                to: 1.0
                                step: 0.05
                                val: root.effectAutoEnhance
                                onMv: function (v) {
                                    root.effectAutoEnhance = v
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: aeHint.implicitHeight + 10
                                color: Qt.rgba(0.8, 0.5, 0.1, 0.12)
                                radius: 4
                                border.color: Qt.rgba(0.9, 0.6, 0.1, 0.4)
                                border.width: 1
                                Text {
                                    id: aeHint
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    text: "✨ Повышает резкость, контраст и насыщенность одним ползунком"
                                    color: "#FFD54F"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // РАЗМЫТИЕ
                        FxSlider {
                            visible: root.selectedEffect === "Размытие"
                            label: "Радиус"
                            valText: root.effectBlur.toFixed(1) + " px"
                            from: 0
                            to: 10
                            step: .5
                            val: root.effectBlur
                            onMv: function (v) {
                                root.effectBlur = v
                            }
                        }

                        // РЕЗКОСТЬ
                        FxSlider {
                            visible: root.selectedEffect === "Резкость"
                            label: "Сила резкости"
                            valText: root.effectSharpness.toFixed(1) + "x"
                            from: 0
                            to: 5
                            step: .1
                            val: root.effectSharpness
                            onMv: function (v) {
                                root.effectSharpness = v
                            }
                        }

                        // ВИНЬЕТКА
                        FxSlider {
                            visible: root.selectedEffect === "Виньетка"
                            label: "Затемнение краёв"
                            valText: Math.round(root.effectVignette * 100) + "%"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectVignette
                            onMv: function (v) {
                                root.effectVignette = v
                            }
                        }

                        // ГРОМКОСТЬ
                        FxSlider {
                            visible: root.selectedEffect === "Громкость"
                            label: "Уровень"
                            valText: Math.round(root.effectVolume * 100) + "%"
                            from: 0
                            to: 2
                            step: .05
                            val: root.effectVolume
                            onMv: function (v) {
                                root.effectVolume = v
                            }
                        }

                        // НОРМАЛИЗАЦИЯ
                        Column {
                            visible: root.selectedEffect === "Нормализация"
                            spacing: 4
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxSlider {
                                label: "Целевой уровень"
                                valText: Math.round(
                                             root.effectNormalize * 100) + "%"
                                from: .1
                                to: 1
                                step: .05
                                val: root.effectNormalize
                                onMv: function (v) {
                                    root.effectNormalize = v
                                }
                            }
                            Text {
                                text: "Авто-усиление до заданного пика"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        // ФЕЙД-ИН
                        FxSlider {
                            visible: root.selectedEffect === "Фейд-ин"
                            label: "Нарастание"
                            valText: Math.round(
                                         root.effectFadeIn * 100) + "% длины клипа"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectFadeIn
                            onMv: function (v) {
                                root.effectFadeIn = v
                            }
                        }

                        // ФЕЙД-АУТ
                        FxSlider {
                            visible: root.selectedEffect === "Фейд-аут"
                            label: "Затухание"
                            valText: Math.round(
                                         root.effectFadeOut * 100) + "% длины клипа"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectFadeOut
                            onMv: function (v) {
                                root.effectFadeOut = v
                            }
                        }

                        // РЕВЕРБЕРАЦИЯ
                        FxSlider {
                            visible: root.selectedEffect === "Реверберация"
                            label: "Размер комнаты"
                            valText: Math.round(root.effectReverb * 100) + "%"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectReverb
                            onMv: function (v) {
                                root.effectReverb = v
                            }
                        }

                        // ЭХО
                        FxSlider {
                            visible: root.selectedEffect === "Эхо"
                            label: "Задержка"
                            valText: Math.round(
                                         150 + root.effectEcho * 350) + " мс"
                            from: 0
                            to: 1
                            step: .05
                            val: root.effectEcho
                            onMv: function (v) {
                                root.effectEcho = v
                            }
                        }

                        // МОНО
                        Column {
                            visible: root.selectedEffect === "Моно"
                            spacing: 4
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxCheck {
                                label: "Конвертировать в моно"
                                chk: root.effectMono
                                onTg: function () {
                                    root.effectMono = !root.effectMono
                                }
                            }
                            Text {
                                text: "Оба канала получат одинаковый сигнал"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        // СТЕРЕО
                        Column {
                            visible: root.selectedEffect === "Стерео"
                            spacing: 4
                            width: parent.width
                            height: visible ? implicitHeight : 0
                            FxSlider {
                                label: "Ширина стерео"
                                valText: Math.round(
                                             root.effectStereoWiden * 100) + "%"
                                from: 0
                                to: 1
                                step: .05
                                val: root.effectStereoWiden
                                onMv: function (v) {
                                    root.effectStereoWiden = v
                                }
                            }
                            Text {
                                text: "0% = моно, 100% = широкое стерео (M/S обработка)"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        // ПИТЧ
                        FxSlider {
                            visible: root.selectedEffect === "Питч"
                            label: "Высота тона"
                            valText: (root.effectPitch >= 0 ? "+" : "") + root.effectPitch.toFixed(
                                         1) + " пт"
                            from: -12
                            to: 12
                            step: .5
                            val: root.effectPitch
                            onMv: function (v) {
                                root.effectPitch = v
                            }
                        }

                        // Кнопки применить/сброс
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: root.selectedClipId >= 0
                            Button {
                                Layout.fillWidth: true
                                implicitHeight: 28
                                text: "✓ Применить"
                                onClicked: root.applyCurrentEffect()
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
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
                            Button {
                                implicitWidth: 72
                                implicitHeight: 28
                                text: "↺ Сброс"
                                onClicked: root.resetAllEffects()
                                contentItem: Text {
                                    text: parent.text
                                    color: "#EF5350"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
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

                //Список эффектов
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    visible: root.mainTab !== 2

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6
                                radius: 3
                                color: Theme.rubyPrimary
                                opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                            }
                        }
                        Column {
                            width: parent.parent.width - 14
                            spacing: 0

                            // ВИДЕО — Цвет и тон
                            FxCategory {
                                visible: root.mainTab === 0
                                         && root.videoSubTab === 0
                                catTitle: "Цвет и тон"
                                catEffects: [{
                                        "name": "Яркость",
                                        "icon": "☀️"
                                    }, {
                                        "name": "Контраст",
                                        "icon": "🔆"
                                    }, {
                                        "name": "Насыщенность",
                                        "icon": "🎨"
                                    }, {
                                        "name": "Оттенок",
                                        "icon": "🌈"
                                    }, {
                                        "name": "Температура",
                                        "icon": "🌡️"
                                    }, {
                                        "name": "Тинт",
                                        "icon": "🖌️"
                                    }, {
                                        "name": "Сепия",
                                        "icon": "🟤"
                                    }, {
                                        "name": "Ч/Б",
                                        "icon": "◻️"
                                    }, {
                                        "name": "Авто-улучшение",
                                        "icon": "✨"
                                    }]
                            }

                            // ВИДЕО — Стилизация
                            FxCategory {
                                visible: root.mainTab === 0
                                         && root.videoSubTab === 1
                                catTitle: "Стилизация"
                                catEffects: [{
                                        "name": "Инверсия",
                                        "icon": "🔄"
                                    }, {
                                        "name": "Постеризация",
                                        "icon": "🎭"
                                    }, {
                                        "name": "Пикселизация",
                                        "icon": "⊞"
                                    }, {
                                        "name": "Зернистость",
                                        "icon": "📽️"
                                    }, {
                                        "name": "Хромакей",
                                        "icon": "💚"
                                    }]
                            }
                            // Подсказка хромакей
                            Rectangle {
                                visible: root.mainTab === 0
                                         && root.videoSubTab === 1
                                width: parent.width
                                height: ckHint.implicitHeight + 12
                                color: Qt.rgba(0, .5, .1, .12)
                                radius: 6
                                border.color: Qt.rgba(0, .8, .2, .35)
                                border.width: 1
                                Text {
                                    id: ckHint
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    text: "💚 Хромакей: видео с зелёным фоном на Дорожку 1, фон — на Дорожку 2"
                                    color: "#70d870"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }

                            // ВИДЕО — Фокус
                            FxCategory {
                                visible: root.mainTab === 0
                                         && root.videoSubTab === 2
                                catTitle: "Резкость и фокус"
                                catEffects: [{
                                        "name": "Размытие",
                                        "icon": "◎"
                                    }, {
                                        "name": "Резкость",
                                        "icon": "⬥"
                                    }, {
                                        "name": "Виньетка",
                                        "icon": "🔳"
                                    }]
                            }

                            // АУДИО — Динамика
                            FxCategory {
                                visible: root.mainTab === 1
                                         && root.audioSubTab === 0
                                catTitle: "Динамика"
                                catEffects: [{
                                        "name": "Громкость",
                                        "icon": "🔊"
                                    }, {
                                        "name": "Нормализация",
                                        "icon": "📊"
                                    }, {
                                        "name": "Фейд-ин",
                                        "icon": "📈"
                                    }, {
                                        "name": "Фейд-аут",
                                        "icon": "📉"
                                    }]
                            }

                            // АУДИО — Пространство
                            FxCategory {
                                visible: root.mainTab === 1
                                         && root.audioSubTab === 1
                                catTitle: "Пространство и эффекты"
                                catEffects: [{
                                        "name": "Реверберация",
                                        "icon": "〰️"
                                    }, {
                                        "name": "Эхо",
                                        "icon": "↩️"
                                    }, {
                                        "name": "Стерео",
                                        "icon": "🎧"
                                    }, {
                                        "name": "Моно",
                                        "icon": "🔈"
                                    }]
                            }

                            // АУДИО — Тон
                            FxCategory {
                                visible: root.mainTab === 1
                                         && root.audioSubTab === 2
                                catTitle: "Тон"
                                catEffects: [{
                                        "name": "Питч",
                                        "icon": "🎵"
                                    }]
                            }
                        }
                    }
                }

                // ПЕРЕХОДЫ
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.mainTab === 2

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            text: "ПЕРЕХОДЫ"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }

                        Text {
                            visible: root.selectedClipId < 0
                            text: "⚠️ Выберите клип на таймлайне"
                            color: "#FFA726"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        // Длительность
                        Rectangle {
                            Layout.fillWidth: true
                            height: durC.implicitHeight + 14
                            color: Theme.backgroundDark
                            radius: Theme.borderRadius
                            border.color: Qt.rgba(0.8, .1, .2, .4)
                            border.width: 1
                            Column {
                                id: durC
                                anchors.fill: parent
                                anchors.margins: 7
                                spacing: 5
                                Text {
                                    text: "Длительность: " + root.transitionDur.toFixed(
                                              2) + " с"
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                Slider {
                                    width: parent.width
                                    from: .1
                                    to: 2.0
                                    stepSize: .05
                                    value: root.transitionDur
                                    onMoved: root.transitionDur = value
                                    background: Rectangle {
                                        x: parent.leftPadding
                                        y: parent.topPadding + parent.availableHeight / 2 - 2
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
                                        x: parent.leftPadding + parent.visualPosition
                                           * parent.availableWidth - 8
                                        y: parent.topPadding + parent.availableHeight / 2 - 8
                                        width: 16
                                        height: 16
                                        radius: 8
                                        color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary
                                    }
                                }
                            }
                        }

                        // Вход
                        Text {
                            text: "↘  ВХОД КЛИПА"
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 5
                            Repeater {
                                model: [{
                                        "id": "none",
                                        "label": "Нет",
                                        "icon": "⬜"
                                    }, {
                                        "id": "fade_in",
                                        "label": "Появление",
                                        "icon": "🌅"
                                    }, {
                                        "id": "wipe_right",
                                        "label": "Смывка →",
                                        "icon": "▶"
                                    }, {
                                        "id": "zoom_in",
                                        "label": "Приближение",
                                        "icon": "🔍"
                                    }, {
                                        "id": "flash",
                                        "label": "Вспышка",
                                        "icon": "⚡"
                                    }]
                                TransitionCard {
                                    cardLabel: modelData.label
                                    cardIcon: modelData.icon
                                    isActive: root.transitionIn === modelData.id
                                    onActivated: {
                                        root.transitionIn = modelData.id
                                        if (cppTimeline
                                                && root.selectedClipId >= 0
                                                && cppTimeline.setTransition)
                                            cppTimeline.setTransition(
                                                        root.selectedClipId,
                                                        "in", modelData.id,
                                                        root.transitionDur)
                                    }
                                }
                            }
                        }

                        // Выход
                        Text {
                            text: "↗  ВЫХОД КЛИПА"
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 5
                            Repeater {
                                model: [{
                                        "id": "none",
                                        "label": "Нет",
                                        "icon": "⬜"
                                    }, {
                                        "id": "fade_out",
                                        "label": "Затухание",
                                        "icon": "🌆"
                                    }, {
                                        "id": "wipe_left",
                                        "label": "Смывка ←",
                                        "icon": "◀"
                                    }, {
                                        "id": "zoom_out",
                                        "label": "Отдаление",
                                        "icon": "🔎"
                                    }, {
                                        "id": "flash",
                                        "label": "Вспышка",
                                        "icon": "⚡"
                                    }]
                                TransitionCard {
                                    cardLabel: modelData.label
                                    cardIcon: modelData.icon
                                    isActive: root.transitionOut === modelData.id
                                    onActivated: {
                                        root.transitionOut = modelData.id
                                        if (cppTimeline
                                                && root.selectedClipId >= 0
                                                && cppTimeline.setTransition)
                                            cppTimeline.setTransition(
                                                        root.selectedClipId,
                                                        "out", modelData.id,
                                                        root.transitionDur)
                                    }
                                }
                            }
                        }

                        // Кнопки применить переходы
                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.selectedClipId >= 0
                            spacing: 8

                            Button {
                                Layout.fillWidth: true
                                implicitHeight: 32
                                text: "✓ Применить переходы"
                                onClicked: {
                                    if (!cppTimeline || root.selectedClipId < 0)
                                        return
                                    var id = root.selectedClipId
                                    var inCode = {
                                        "none": 0,
                                        "fade_in": 1,
                                        "wipe_right": 2,
                                        "wipe_left": 3,
                                        "zoom_in": 4,
                                        "zoom_out": 5,
                                        "flash": 6
                                    }
                                    var outCode = {
                                        "none": 0,
                                        "fade_out": 1,
                                        "wipe_right": 2,
                                        "wipe_left": 3,
                                        "zoom_in": 4,
                                        "zoom_out": 5,
                                        "flash": 6
                                    }
                                    cppTimeline.applyEffect(
                                                id, "transition_in",
                                                inCode[root.transitionIn] || 0)
                                    cppTimeline.applyEffect(
                                                id, "transition_out",
                                                outCode[root.transitionOut]
                                                || 0)
                                    cppTimeline.applyEffect(
                                                id, "transition_duration",
                                                root.transitionDur)
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
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
                            Button {
                                implicitWidth: 72
                                implicitHeight: 32
                                text: "↺ Убрать"
                                onClicked: {
                                    if (!cppTimeline || root.selectedClipId < 0)
                                        return
                                    var id = root.selectedClipId
                                    cppTimeline.removeEffect(id,
                                                             "transition_in")
                                    cppTimeline.removeEffect(id,
                                                             "transition_out")
                                    cppTimeline.removeEffect(
                                                id, "transition_duration")
                                    root.transitionIn = "none"
                                    root.transitionOut = "none"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#EF5350"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
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

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }

        // EXPORT MODE: a simple settings form, doesn't export anything
        // itself — just collects the user's choices and emits
        // exportRequested upward (the actual export is done by main.qml → C++)
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

    //  КОМПОНЕНТЫ — все используют id-ссылки вместо parent.parent цепочек

    //  Labeled slider: the generic effect slider, used for every
    // continuous numeric parameter (brightness, contrast, etc.).
    // rainbow/warm — two special track-background modes for specific
    // effects (Hue — rainbow gradient, Temperature — blue-orange
    // gradient); with both false, a plain filled progress bar is drawn
    component FxSlider: Item {
        id: fxSlRoot
        property string label: ""
        property string valText: ""
        property real from: 0
        property real to: 1
        property real step: 0.05
        property real val: 0
        property bool rainbow: false
        property bool warm: false
        signal mv(real v)

        width: parent ? parent.width : 0
        // Когда invisible — не занимает место в layout
        height: visible ? slCol.implicitHeight : 0

        Column {
            id: slCol
            width: parent.width
            spacing: 3

            Text {
                text: fxSlRoot.label + ": " + fxSlRoot.valText
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
                width: parent.width
            }
            Slider {
                id: slCtrl
                width: parent.width
                from: fxSlRoot.from
                to: fxSlRoot.to
                stepSize: fxSlRoot.step
                value: fxSlRoot.val
                onMoved: fxSlRoot.mv(value)

                background: Rectangle {
                    x: slCtrl.leftPadding
                    y: slCtrl.topPadding + slCtrl.availableHeight / 2 - height / 2
                    width: slCtrl.availableWidth
                    height: 4
                    radius: 2
                    color: fxSlRoot.rainbow
                           || fxSlRoot.warm ? "transparent" : Theme.backgroundDark

                    gradient: fxSlRoot.rainbow ? rainbowG : (fxSlRoot.warm ? warmG : null)

                    Gradient {
                        id: rainbowG
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: "#FF0000"
                        }
                        GradientStop {
                            position: 0.167
                            color: "#FFFF00"
                        }
                        GradientStop {
                            position: 0.333
                            color: "#00FF00"
                        }
                        GradientStop {
                            position: 0.5
                            color: "#00FFFF"
                        }
                        GradientStop {
                            position: 0.667
                            color: "#0000FF"
                        }
                        GradientStop {
                            position: 0.833
                            color: "#FF00FF"
                        }
                        GradientStop {
                            position: 1.0
                            color: "#FF0000"
                        }
                    }
                    Gradient {
                        id: warmG
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: "#4488FF"
                        }
                        GradientStop {
                            position: 0.5
                            color: "#333333"
                        }
                        GradientStop {
                            position: 1.0
                            color: "#FF8844"
                        }
                    }
                    Rectangle {
                        visible: !fxSlRoot.rainbow && !fxSlRoot.warm
                        width: slCtrl.visualPosition * parent.width
                        height: parent.height
                        color: Theme.rubyPrimary
                        radius: 2
                    }
                }
                handle: Rectangle {
                    x: slCtrl.leftPadding + slCtrl.visualPosition
                       * slCtrl.availableWidth - width / 2
                    y: slCtrl.topPadding + slCtrl.availableHeight / 2 - height / 2
                    width: 16
                    height: 16
                    radius: 8
                    color: slCtrl.pressed ? Theme.rubyLight : Theme.rubyPrimary
                }
            }
        }
    }

    // Labeled checkbox: for boolean effects (Grayscale, Invert, Mono, etc.)
    component FxCheck: Item {
        id: fxChkRoot
        property string label: ""
        property bool chk: false
        signal tg

        width: parent ? parent.width : 0
        height: visible ? 24 : 0

        RowLayout {
            anchors.fill: parent
            spacing: 8

            Rectangle {
                id: chkBox
                width: 18
                height: 18
                radius: 3
                color: fxChkRoot.chk ? Theme.rubyPrimary : Theme.backgroundDark
                border.color: Theme.rubyPrimary
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    visible: fxChkRoot.chk
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: fxChkRoot.tg()
                }
            }

            Text {
                text: fxChkRoot.label
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                MouseArea {
                    anchors.fill: parent
                    onClicked: fxChkRoot.tg()
                }
            }
        }
    }

    //  Effect category: a collapsible group (collapsed) listing the
    // effects in this sub-category; selecting an effect in the list sets
    // root.selectedEffect, which applyCurrentEffect() later reads
    component FxCategory: Column {
        id: fxCatRoot
        property string catTitle: ""
        property var catEffects: []
        property bool collapsed: false

        width: parent ? parent.width : 0
        spacing: 0

        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.borderRadius
            color: catHdr.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text {
                    text: fxCatRoot.collapsed ? "▶" : "▼"
                    color: Theme.rubyPrimary
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    text: fxCatRoot.catTitle
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
            MouseArea {
                id: catHdr
                anchors.fill: parent
                hoverEnabled: true
                onClicked: fxCatRoot.collapsed = !fxCatRoot.collapsed
            }
        }

        Column {
            width: parent.width
            spacing: 2
            visible: !fxCatRoot.collapsed

            Repeater {
                model: fxCatRoot.catEffects

                Rectangle {
                    id: efRow
                    width: parent.width
                    height: 34
                    radius: Theme.borderRadius
                    color: efItem.containsMouse ? Qt.rgba(
                                                      0.8, 0.1, 0.2,
                                                      0.2) : (root.selectedEffect === modelData.name ? Qt.rgba(0.8, 0.1, 0.2, 0.12) : "transparent")
                    border.color: root.selectedEffect
                                  === modelData.name ? Theme.rubyPrimary : "transparent"
                    border.width: 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 80
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            text: modelData.icon
                            font.pixelSize: 14
                        }
                        Text {
                            text: modelData.name
                            color: root.selectedEffect
                                   === modelData.name ? Theme.rubyLight : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            Layout.fillWidth: true
                        }
                        // Точка "применён"
                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.rubyPrimary
                            visible: {
                                if (root.selectedClipId < 0 || !cppTimeline)
                                    return false
                                var fx = cppTimeline.getClipEffects ? cppTimeline.getClipEffects(
                                                                          root.selectedClipId) : {}
                                var m = {
                                    "яркость": "brightness",
                                    "контраст": "contrast",
                                    "насыщенность": "saturation",
                                    "оттенок": "hue",
                                    "температура": "temperature",
                                    "тинт": "tint_hue",
                                    "сепия": "sepia",
                                    "ч/б": "grayscale",
                                    "инверсия": "invert",
                                    "постеризация": "posterize",
                                    "пикселизация": "pixelate",
                                    "зернистость": "grain",
                                    "хромакей": "chroma_key",
                                    "размытие": "blur",
                                    "резкость": "sharpness",
                                    "виньетка": "vignette",
                                    "громкость": "volume",
                                    "нормализация": "normalize",
                                    "фейд-ин": "fade_in",
                                    "фейд-аут": "fade_out",
                                    "реверберация": "reverb",
                                    "эхо": "echo",
                                    "моно": "mono",
                                    "стерео": "stereo_widen",
                                    "питч": "pitch"
                                }
                                var k = m[modelData.name.toLowerCase()]
                                return k !== undefined && fx[k] !== undefined
                            }
                        }
                    }
                    MouseArea {
                        id: efItem
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.selectedEffect = modelData.name
                    }
                }
            }
        }
    }

    // Transition card: a clickable tile for choosing a
    // transition_in/transition_out type (Fade in, Wipe, Flash, etc.)
    component TransitionCard: Item {
        id: tcRoot
        property string cardLabel: ""
        property string cardIcon: ""
        property bool isActive: false
        signal activated

        width: 86
        height: 52

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: tcRoot.isActive ? Qt.rgba(
                                         0.8, 0.1, 0.2,
                                         0.4) : (tcMa.containsMouse ? Qt.rgba(
                                                                          1, 1,
                                                                          1,
                                                                          0.07) : Theme.backgroundDark)
            border.color: tcRoot.isActive ? Theme.rubyPrimary : Qt.rgba(1, 1,
                                                                        1, 0.1)
            border.width: tcRoot.isActive ? 2 : 1
            Behavior on color {
                ColorAnimation {
                    duration: 100
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 3

                Text {
                    text: tcRoot.cardIcon
                    font.pixelSize: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: tcRoot.cardLabel
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                    width: 78
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            MouseArea {
                id: tcMa
                anchors.fill: parent
                hoverEnabled: true
                onClicked: tcRoot.activated()
            }
        }
    }

    // Export: the export settings form (resolution + format) — used in
    // the "EXPORT MODE" block above. Values are read directly from the
    // ComboBoxes at the moment the export button is clicked; no separate
    // property state is kept for them
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
                id: resC
                implicitWidth: 150
                model: ["1920×1080", "1280×720", "3840×2160", "2560×1440"]
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
        }
        SettingRow {
            label: "Формат"
            ComboBox {
                id: fmtC
                implicitWidth: 150
                model: ["MP4", "AVI", "MOV", "MKV", "WebM"]
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
        }
        // Saving itself reads the current text of the selected ComboBoxes
        // (resC/fmtC) directly at click time — a simple way to avoid
        // duplicating their state in separate properties
        Button {
            text: "СОХРАНИТЬ ВИДЕО"
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            onClicked: exportClicked(resC.currentText, fmtC.currentText)
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
                        duration: Theme.animationDuration
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
        }
    }

    // "Label + field" row for the export settings form (used with the
    // resolution/format ComboBoxes above)
    component SettingRow: RowLayout {
        property string label: ""
        Layout.fillWidth: true
        spacing: Theme.spacing
        Text {
            text: label
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            Layout.fillWidth: true
        }
    }
}
