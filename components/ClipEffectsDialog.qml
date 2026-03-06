import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

// ===== ДИАЛОГ ЭФФЕКТОВ КЛИПА =====
// Секции: Видео / Аудио / Переходы. Каждый эффект — строка.
Window {
    id: effectsDialog
    title: "Эффекты клипа"
    width: 480
    height: 560
    minimumWidth: 400
    minimumHeight: 420
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal

    property int clipId: -1
    property string clipName: ""
    property var parentWindow: null

    property var videoEffects: []
    property var audioEffects: []
    property var transitionEffects: []
    property int totalCount: videoEffects.length + audioEffects.length + transitionEffects.length

    // ── Мета-данные всех эффектов ──
    readonly property var effectMeta: ({
                                           "brightness": {
                                               "icon": "☀️",
                                               "label": "Яркость",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return (v >= 0 ? "+" : "") + v.toFixed(
                                                               2)
                                               }
                                           },
                                           "contrast": {
                                               "icon": "🔆",
                                               "label": "Контраст",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v.toFixed(2) + "x"
                                               }
                                           },
                                           "saturation": {
                                               "icon": "🎨",
                                               "label": "Насыщенность",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v.toFixed(2) + "x"
                                               }
                                           },
                                           "grayscale": {
                                               "icon": "◻️",
                                               "label": "Ч/Б",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v > 0.5 ? "вкл" : "выкл"
                                               }
                                           },
                                           "hue": {
                                               "icon": "🌈",
                                               "label": "Оттенок",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return (v >= 0 ? "+" : "") + v.toFixed(
                                                               0) + "°"
                                               }
                                           },
                                           "temperature": {
                                               "icon": "🌡️",
                                               "label": "Температура",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v >= 0 ? "тёплый +" + v.toFixed(
                                                                       2) : "холодный " + v.toFixed(
                                                                       2)
                                               }
                                           },
                                           "tint_hue": {
                                               "icon": "🖌️",
                                               "label": "Тинт",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v.toFixed(0) + "°"
                                               }
                                           },
                                           "sepia": {
                                               "icon": "🟤",
                                               "label": "Сепия",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "invert": {
                                               "icon": "🔄",
                                               "label": "Инверсия",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v > 0.5 ? "вкл" : "выкл"
                                               }
                                           },
                                           "posterize": {
                                               "icon": "🎭",
                                               "label": "Постеризация",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v < 2 ? "выкл" : Math.round(
                                                                      v) + " ур."
                                               }
                                           },
                                           "pixelate": {
                                               "icon": "⊞",
                                               "label": "Пикселизация",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v < 2 ? "выкл" : Math.round(
                                                                      v) + " px"
                                               }
                                           },
                                           "grain": {
                                               "icon": "📽️",
                                               "label": "Зернистость",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "chroma_key": {
                                               "icon": "💚",
                                               "label": "Хромакей",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v > 0.5 ? "вкл" : "выкл"
                                               }
                                           },
                                           "blur": {
                                               "icon": "◎",
                                               "label": "Размытие",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v.toFixed(1) + " px"
                                               }
                                           },
                                           "sharpness": {
                                               "icon": "⬥",
                                               "label": "Резкость",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return v.toFixed(1) + "x"
                                               }
                                           },
                                           "vignette": {
                                               "icon": "🔳",
                                               "label": "Виньетка",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "volume": {
                                               "icon": "🔊",
                                               "label": "Громкость",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "reverb": {
                                               "icon": "〰️",
                                               "label": "Реверб",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "echo": {
                                               "icon": "↩️",
                                               "label": "Эхо",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               150 + v * 350) + " мс"
                                               }
                                           },
                                           "mono": {
                                               "icon": "🔈",
                                               "label": "Моно",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return v > 0.5 ? "вкл" : "выкл"
                                               }
                                           },
                                           "stereo_widen": {
                                               "icon": "🎧",
                                               "label": "Стерео",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "pitch": {
                                               "icon": "🎵",
                                               "label": "Питч",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return (v >= 0 ? "+" : "") + v.toFixed(
                                                               1) + " пт"
                                               }
                                           },
                                           "normalize": {
                                               "icon": "📊",
                                               "label": "Нормализация",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           },
                                           "fade_in": {
                                               "icon": "📈",
                                               "label": "Фейд-ин",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "% клипа"
                                               }
                                           },
                                           "fade_out": {
                                               "icon": "📉",
                                               "label": "Фейд-аут",
                                               "cat": "audio",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "% клипа"
                                               }
                                           },
                                           "transition_in": {
                                               "icon": "🌅",
                                               "label": "Вход",
                                               "cat": "transition",
                                               "fmt": function (v) {
                                                   var n = ["Нет", "Появление", "Смывка→", "←Смывка", "Приближение", "Отдаление", "Вспышка"]
                                                   return n[Math.round(
                                                                v)] || "Нет"
                                               }
                                           },
                                           "transition_out": {
                                               "icon": "🌆",
                                               "label": "Выход",
                                               "cat": "transition",
                                               "fmt": function (v) {
                                                   var n = ["Нет", "Затухание", "Смывка→", "←Смывка", "Приближение", "Отдаление", "Вспышка"]
                                                   return n[Math.round(
                                                                v)] || "Нет"
                                               }
                                           },
                                           "transition_duration": {
                                               "icon": "⏱️",
                                               "label": "Длительность",
                                               "cat": "transition",
                                               "fmt": function (v) {
                                                   return v.toFixed(2) + " с"
                                               }
                                           },
                                           "auto_enhance": {
                                               "icon": "✨",
                                               "label": "Авто-улучшение",
                                               "cat": "video",
                                               "fmt": function (v) {
                                                   return Math.round(
                                                               v * 100) + "%"
                                               }
                                           }
                                       })

    function isNeutral(key, val) {
        if (key === "brightness" && Math.abs(val) < 0.01)
            return true
        if (key === "contrast" && Math.abs(val - 1.0) < 0.01)
            return true
        if (key === "saturation" && Math.abs(val - 1.0) < 0.01)
            return true
        if (key === "grayscale" && val < 0.5)
            return true
        if (key === "invert" && val < 0.5)
            return true
        if (key === "mono" && val < 0.5)
            return true
        if (key === "volume" && Math.abs(val - 1.0) < 0.01)
            return true
        if (key === "transition_in" && Math.round(val) === 0)
            return true
        if (key === "transition_out" && Math.round(val) === 0)
            return true
        if (key === "tint_strength")
            return true
        if (key === "chroma_threshold")
            return true
        if (key === "chroma_smoothness")
            return true
        if (key === "_frameIdx")
            return true
        if (key === "_uid")
            return true
        if (key === "auto_enhance" && val < 0.01)
            return true
        return false
    }

    // ===== API =====
    function openForClip(id) {
        clipId = id
        clipName = ""
        if (cppTimeline) {
            var all = cppTimeline.getClipsForTrack(1).concat(
                        cppTimeline.getClipsForTrack(2))
            for (var i = 0; i < all.length; i++) {
                if (all[i].id === id) {
                    clipName = all[i].filename || ("Клип #" + id)
                    break
                }
            }
        }
        if (!clipName)
            clipName = "Клип #" + id
        refreshEffects()
        if (parentWindow) {
            x = parentWindow.x + (parentWindow.width - width) / 2
            y = parentWindow.y + (parentWindow.height - height) / 2
        }
        visible = true
        raise()
        requestActivate()
    }

    function refreshEffects() {
        if (!cppTimeline || clipId < 0) {
            videoEffects = []
            audioEffects = []
            transitionEffects = []
            return
        }
        var fx = cppTimeline.getClipEffects(clipId)
        var vid = [], aud = [], tr = []

        // Обычные эффекты
        for (var key in fx) {
            var val = fx[key]
            if (isNeutral(key, val))
                continue
            var meta = effectMeta[key]
            if (!meta)
                continue
            // Переходы — собираем отдельно (merge ниже)
            if (meta.cat === "transition")
                continue
            var entry = {
                "key": key,
                "label": meta.label,
                "icon": meta.icon,
                "valStr": meta.fmt(val)
            }
            if (meta.cat === "video")
                vid.push(entry)
            else if (meta.cat === "audio")
                aud.push(entry)
        }

        // Переходы: объединяем transition_in / transition_out / duration в 2 строки макс
        var trNames = ["Нет", "Появление", "Смывка→", "←Смывка", "Приближение", "Отдаление", "Вспышка"]
        var trOutNames = ["Нет", "Затухание", "Смывка→", "←Смывка", "Приближение", "Отдаление", "Вспышка"]
        var dur = fx["transition_duration"] !== undefined ? fx["transition_duration"] : 0.5

        if (fx["transition_in"] !== undefined && Math.round(
                    fx["transition_in"]) !== 0) {
            var inName = trNames[Math.round(fx["transition_in"])] || "?"
            tr.push({
                        "key": "transition_in",
                        "label": "Вход клипа",
                        "icon": "🌅",
                        "valStr": inName + " · " + dur.toFixed(2) + "с",
                        "extraKey": "transition_duration"
                    })
        }
        if (fx["transition_out"] !== undefined && Math.round(
                    fx["transition_out"]) !== 0) {
            var outName = trOutNames[Math.round(fx["transition_out"])] || "?"
            tr.push({
                        "key": "transition_out",
                        "label": "Выход клипа",
                        "icon": "🌆",
                        "valStr": outName + " · " + dur.toFixed(2) + "с",
                        "extraKey": "transition_duration"
                    })
        }

        videoEffects = vid
        audioEffects = aud
        transitionEffects = tr
    }

    function removeEffect(key) {
        if (!cppTimeline || clipId < 0)
            return
        cppTimeline.removeEffect(clipId, key)
        if (key === "chroma_key") {
            cppTimeline.removeEffect(clipId, "chroma_threshold")
            cppTimeline.removeEffect(clipId, "chroma_smoothness")
        }
        if (key === "tint_hue")
            cppTimeline.removeEffect(clipId, "tint_strength")
        // При удалении одного перехода - удаляем duration только если оба перехода убраны
        if (key === "transition_in" || key === "transition_out") {
            var fx = cppTimeline.getClipEffects(clipId)
            var hasIn = key === "transition_in" ? false : (fx["transition_in"] !== undefined
                                                           && Math.round(
                                                               fx["transition_in"]) !== 0)
            var hasOut = key === "transition_out" ? false : (fx["transition_out"] !== undefined
                                                             && Math.round(
                                                                 fx["transition_out"]) !== 0)
            if (!hasIn && !hasOut)
                cppTimeline.removeEffect(clipId, "transition_duration")
        }
        refreshEffects()
    }

    function resetAll() {
        if (!cppTimeline || clipId < 0)
            return
        var keys = ["brightness", "contrast", "saturation", "grayscale", "blur", "sharpness", "hue", "sepia", "vignette", "invert", "posterize", "pixelate", "temperature", "tint_hue", "tint_strength", "grain", "chroma_key", "chroma_threshold", "chroma_smoothness", "volume", "reverb", "echo", "mono", "stereo_widen", "pitch", "normalize", "fade_in", "fade_out", "transition_in", "transition_out", "transition_duration"]
        for (var i = 0; i < keys.length; i++)
            cppTimeline.removeEffect(clipId, keys[i])
        refreshEffects()
    }

    function closeDialog() {
        visible = false
        clipId = -1
        clipName = ""
    }

    // ===== РАЗМЕТКА =====
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
        radius: Theme.borderRadius + 2
        border.color: Theme.rubyPrimary
        border.width: 2

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── Заголовок ──
            Rectangle {
                Layout.fillWidth: true
                height: 50
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
                        text: "✨"
                        font.pixelSize: 20
                    }
                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: "Эффекты клипа"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLarge
                            font.bold: true
                        }
                        Text {
                            text: effectsDialog.clipName
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
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
                                effectsDialog.x += e.x - sx
                                effectsDialog.y += e.y - sy
                            }
                        }
                    }
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: closeMa.containsMouse ? Theme.rubyPrimary : "transparent"
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
                            id: closeMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: effectsDialog.closeDialog()
                        }
                    }
                }
            }

            // ── Счётчик ──
            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: Qt.rgba(1, 1, 1, 0.03)
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8
                    Text {
                        text: "🎬"
                        font.pixelSize: 14
                    }
                    Text {
                        text: "ID " + (effectsDialog.clipId >= 0 ? effectsDialog.clipId : "—")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Rectangle {
                        width: 1
                        height: 14
                        color: Theme.borderLight
                    }
                    Text {
                        text: effectsDialog.totalCount
                              > 0 ? effectsDialog.totalCount + " эффект(ов)" : "Без эффектов"
                        color: effectsDialog.totalCount > 0 ? Theme.rubyLight : Theme.textDisabled
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.borderLight
            }

            // ── Тело ──
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                // Пустое состояние
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: effectsDialog.totalCount === 0
                    Text {
                        text: "🎞️"
                        font.pixelSize: 42
                        Layout.alignment: Qt.AlignHCenter
                        opacity: 0.22
                    }
                    Text {
                        text: "Эффекты не применены"
                        color: Theme.textDisabled
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: "Настройте эффект в левой панели\nи нажмите «✓ Применить»"
                        color: Theme.textDisabled
                        opacity: 0.6
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                ScrollView {
                    id: effectsScrollView
                    anchors.fill: parent
                    clip: true
                    visible: effectsDialog.totalCount > 0
                    contentWidth: availableWidth // фикс Qt6: явная ширина контента
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 5
                            radius: 3
                            color: Theme.rubyPrimary
                            opacity: 0.6
                        }
                    }

                    Column {
                        width: effectsScrollView.availableWidth
                        topPadding: 8
                        bottomPadding: 8
                        spacing: 0

                        // ВИДЕО
                        Column {
                            width: parent.width
                            spacing: 0
                            visible: effectsDialog.videoEffects.length > 0

                            Rectangle {
                                width: parent.width
                                height: 26
                                color: Qt.rgba(0.8, 0.1, 0.2, 0.15)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    spacing: 6
                                    Text {
                                        text: "🎬"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "ВИДЕО"
                                        color: Theme.rubyLight
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Text {
                                        text: effectsDialog.videoEffects.length + " эфф."
                                        color: Theme.textSecondary
                                        font.pixelSize: 9
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            Repeater {
                                model: effectsDialog.videoEffects
                                delegate: EffectRow {
                                    effectData: modelData
                                    eWidth: effectsScrollView.availableWidth
                                }
                            }
                        }

                        // АУДИО
                        Column {
                            width: parent.width
                            spacing: 0
                            visible: effectsDialog.audioEffects.length > 0

                            Rectangle {
                                width: parent.width
                                height: 26
                                color: Qt.rgba(0.1, 0.3, 0.9, 0.12)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    spacing: 6
                                    Text {
                                        text: "🎵"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "АУДИО"
                                        color: "#7CA8FF"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Text {
                                        text: effectsDialog.audioEffects.length + " эфф."
                                        color: Theme.textSecondary
                                        font.pixelSize: 9
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            Repeater {
                                model: effectsDialog.audioEffects
                                delegate: EffectRow {
                                    effectData: modelData
                                    eWidth: effectsScrollView.availableWidth
                                }
                            }
                        }

                        // ПЕРЕХОДЫ
                        Column {
                            width: parent.width
                            spacing: 0
                            visible: effectsDialog.transitionEffects.length > 0

                            Rectangle {
                                width: parent.width
                                height: 26
                                color: Qt.rgba(0.9, 0.7, 0.1, 0.12)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    spacing: 6
                                    Text {
                                        text: "✨"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "ПЕРЕХОДЫ"
                                        color: "#FFD54F"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            Repeater {
                                model: effectsDialog.transitionEffects
                                delegate: EffectRow {
                                    effectData: modelData
                                    eWidth: effectsScrollView.availableWidth
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.borderLight
            }

            // ── Кнопки ──
            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: Theme.backgroundDark
                radius: Theme.borderRadius + 2
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: parent.radius
                    color: parent.color
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8
                    Button {
                        text: "🗑  Сбросить все"
                        enabled: effectsDialog.totalCount > 0
                        Layout.preferredHeight: 32
                        onClicked: effectsDialog.resetAll()
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#EF5350" : Theme.textDisabled
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.borderRadius
                            color: parent.down ? Qt.rgba(
                                                     .94, .33, .31,
                                                     .3) : (parent.hovered ? Qt.rgba(.94, .33, .31, .12) : "transparent")
                            border.color: "#EF5350"
                            border.width: 1
                            opacity: parent.enabled ? 1 : 0.4
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Закрыть"
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 88
                        onClicked: effectsDialog.closeDialog()
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
                            radius: Theme.borderRadius
                            color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary)
                            Behavior on color {
                                ColorAnimation {
                                    duration: 100
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════
    //  КОМПОНЕНТ: Строка одного эффекта
    // ═══════════════════════════════════
    component EffectRow: Rectangle {
        id: efRowRoot
        property var effectData: null
        property real eWidth: 460

        width: eWidth
        height: 42
        color: rowHover.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"

        // Левая полоска
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 2
            height: 26
            radius: 1
            color: Theme.rubyPrimary
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 10
            spacing: 10

            // Иконка
            Text {
                text: efRowRoot.effectData ? efRowRoot.effectData.icon : "⚙️"
                font.pixelSize: 16
                width: 22
                horizontalAlignment: Text.AlignHCenter
            }

            // Название
            Text {
                text: efRowRoot.effectData ? efRowRoot.effectData.label : ""
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                font.bold: true
                Layout.preferredWidth: 130
                elide: Text.ElideRight
            }

            // Значение — бейдж
            Rectangle {
                height: 22
                Layout.preferredWidth: valBadge.implicitWidth + 18
                radius: 4
                color: Qt.rgba(0.8, 0.1, 0.2, 0.18)
                border.color: Qt.rgba(0.8, 0.1, 0.2, 0.45)
                border.width: 1
                Text {
                    id: valBadge
                    anchors.centerIn: parent
                    text: efRowRoot.effectData ? efRowRoot.effectData.valStr : ""
                    color: Theme.rubyLight
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // Удалить
            Rectangle {
                width: 24
                height: 24
                radius: 5
                color: delMa.containsMouse ? Qt.rgba(.94, .33, .31,
                                                     .25) : "transparent"
                border.color: Qt.rgba(.94, .33, .31,
                                      delMa.containsMouse ? 0.8 : 0.3)
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: 80
                    }
                }
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "#EF5350"
                    font.pixelSize: 10
                    font.bold: true
                }
                MouseArea {
                    id: delMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (efRowRoot.effectData)
                            effectsDialog.removeEffect(efRowRoot.effectData.key)
                    }
                }
            }
        }

        // Разделитель
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            height: 1
            color: Theme.borderLight
            opacity: 0.25
        }

        MouseArea {
            id: rowHover
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
    }
}
