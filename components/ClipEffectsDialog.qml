import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

// ===== ПАНЕЛЬ ЭФФЕКТОВ КЛИПА =====
// Вызывается из VideoClip.qml через onEffectsRequested.
// Подключение в main.qml:
//   ClipEffectsDialog {
//       id: clipEffectsDialog
//       parentWindow: root  // ← Window
//   }
// И в Timeline → Track → VideoClip:
//   onEffectsRequested: id => clipEffectsDialog.openForClip(id)
Window {
    id: effectsDialog
    title: "Эффекты клипа"
    width: 500
    height: 580
    minimumWidth: 420
    minimumHeight: 460
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal

    property int clipId: -1
    property string clipName: ""
    property var parentWindow: null
    property var appliedEffects: []

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

        // Загружаем реальные эффекты из C++
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
            appliedEffects = []
            return
        }
        var fx = cppTimeline.getClipEffects(clipId)
        var arr = []
        var names = {
            "brightness": {
                "icon": "☀️",
                "label": "Яркость"
            },
            "contrast": {
                "icon": "🔆",
                "label": "Контраст"
            },
            "saturation": {
                "icon": "🎨",
                "label": "Насыщенность"
            },
            "grayscale": {
                "icon": "⬜",
                "label": "Чёрно-белое"
            }
        }
        for (var key in fx) {
            var val = fx[key]
            // Пропускаем нейтральные значения
            if (key === "brightness" && Math.abs(val) < 0.01)
                continue
            if (key === "contrast" && Math.abs(val - 1.0) < 0.01)
                continue
            if (key === "saturation" && Math.abs(val - 1.0) < 0.01)
                continue
            if (key === "grayscale" && val < 0.5)
                continue
            var info = names[key] || {
                "icon": "⚙️",
                "label": key
            }
            arr.push({
                         "id": key,
                         "name": info.label,
                         "icon": info.icon,
                         "params": key !== "grayscale" ? val.toFixed(2) : "вкл"
                     })
        }
        appliedEffects = arr
    }

    function closeDialog() {
        visible = false
        clipId = -1
        clipName = ""
    }

    // ===== UI =====
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
                height: 48
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
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
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
                        onPressed: {
                            sx = mouseX
                            sy = mouseY
                        }
                        onPositionChanged: {
                            if (pressed) {
                                effectsDialog.x += mouseX - sx
                                effectsDialog.y += mouseY - sy
                            }
                        }
                    }
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: closeBtnMA.containsMouse ? Theme.rubyPrimary : "transparent"
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
                            id: closeBtnMA
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: effectsDialog.closeDialog()
                        }
                    }
                }
            }

            // ── Инфо + пресеты ──
            Rectangle {
                Layout.fillWidth: true
                height: 46
                color: Qt.rgba(1, 1, 1, 0.03)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Text {
                        text: "🎬"
                        font.pixelSize: 16
                    }
                    Text {
                        text: "ID " + (effectsDialog.clipId >= 0 ? effectsDialog.clipId : "—")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Rectangle {
                        width: 1
                        height: 18
                        color: Theme.borderLight
                    }
                    Text {
                        text: effectsDialog.appliedEffects.length
                              > 0 ? effectsDialog.appliedEffects.length
                                    + " эффект(ов)" : "Без эффектов"
                        color: effectsDialog.appliedEffects.length
                               > 0 ? Theme.rubyLight : Theme.textDisabled
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Пресеты:"
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                    Repeater {
                        model: [{
                                "label": "Ч/Б",
                                "data": {
                                    "icon": "⬜",
                                    "name": "Grayscale",
                                    "params": "",
                                    "id": 100
                                }
                            }, {
                                "label": "Sepia",
                                "data": {
                                    "icon": "🟤",
                                    "name": "Sepia Tone",
                                    "params": "intensity=0.8",
                                    "id": 101
                                }
                            }, {
                                "label": "Ярче",
                                "data": {
                                    "icon": "☀️",
                                    "name": "Brightness",
                                    "params": "value=+40%",
                                    "id": 102
                                }
                            }, {
                                "label": "Blur",
                                "data": {
                                    "icon": "💧",
                                    "name": "Blur",
                                    "params": "radius=5",
                                    "id": 103
                                }
                            }]
                        Rectangle {
                            height: 26
                            width: pt.implicitWidth + 16
                            radius: 4
                            color: pma.containsMouse ? Theme.rubyPrimary : Theme.backgroundDark
                            border.color: Theme.rubyPrimary
                            border.width: 1
                            Behavior on color {
                                ColorAnimation {
                                    duration: 80
                                }
                            }
                            Text {
                                id: pt
                                anchors.centerIn: parent
                                text: modelData.label
                                color: Theme.textPrimary
                                font.pixelSize: 11
                            }
                            MouseArea {
                                id: pma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var arr = effectsDialog.appliedEffects.slice()
                                    for (var k = 0; k < arr.length; k++)
                                        if (arr[k].id === modelData.data.id)
                                            return
                                    arr.push(modelData.data)
                                    effectsDialog.appliedEffects = arr
                                    // TODO: cppTimeline.addEffect(effectsDialog.clipId, modelData.data)
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

            // ── Список эффектов ──
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                // Пусто
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    visible: effectsDialog.appliedEffects.length === 0
                    Text {
                        text: "🎞️"
                        font.pixelSize: 48
                        Layout.alignment: Qt.AlignHCenter
                        opacity: 0.3
                    }
                    Text {
                        text: "Эффекты не применены"
                        color: Theme.textDisabled
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: "Настройте эффект в левой панели\nи нажмите «Применить к клипу»,\nили добавьте пресет выше."
                        color: Theme.textDisabled
                        opacity: 0.7
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // Список
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    visible: effectsDialog.appliedEffects.length > 0
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
                        width: parent.width
                        topPadding: 6
                        bottomPadding: 6
                        spacing: 0

                        Repeater {
                            model: effectsDialog.appliedEffects

                            delegate: Rectangle {
                                width: parent.width - 12
                                x: 6
                                height: 64
                                color: rh.containsMouse ? Qt.rgba(
                                                              1, 1, 1,
                                                              0.05) : "transparent"
                                radius: Theme.borderRadius

                                // Цветная полоска
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.topMargin: 10
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 10
                                    width: 3
                                    radius: 2
                                    color: Theme.rubyPrimary
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    anchors.margins: 6
                                    spacing: 10

                                    // Иконка
                                    Rectangle {
                                        width: 36
                                        height: 36
                                        radius: 8
                                        color: Qt.rgba(Theme.rubyPrimary.r,
                                                       Theme.rubyPrimary.g,
                                                       Theme.rubyPrimary.b,
                                                       0.15)
                                        border.color: Theme.rubyPrimary
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.icon || "⚙️"
                                            font.pixelSize: 18
                                        }
                                    }

                                    // Название + параметры
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Text {
                                            text: modelData.name
                                            color: Theme.textPrimary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSize
                                            font.bold: true
                                        }
                                        Text {
                                            visible: (modelData.params
                                                      || "") !== ""
                                            text: modelData.params || ""
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.italic: true
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // ▲▼ переставить
                                    ColumnLayout {
                                        spacing: 2
                                        Text {
                                            text: "▲"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: uma.containsMouse ? Theme.textPrimary : Theme.textDisabled
                                            MouseArea {
                                                id: uma
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (index > 0) {
                                                        var a = effectsDialog.appliedEffects.slice()
                                                        var t = a[index - 1]
                                                        a[index - 1] = a[index]
                                                        a[index] = t
                                                        effectsDialog.appliedEffects = a
                                                    }
                                                }
                                            }
                                        }
                                        Text {
                                            text: "▼"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: dma2.containsMouse ? Theme.textPrimary : Theme.textDisabled
                                            MouseArea {
                                                id: dma2
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    var a = effectsDialog.appliedEffects.slice()
                                                    if (index < a.length - 1) {
                                                        var t2 = a[index + 1]
                                                        a[index + 1] = a[index]
                                                        a[index] = t2
                                                        effectsDialog.appliedEffects = a
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Удалить
                                    Rectangle {
                                        width: 28
                                        height: 28
                                        radius: 6
                                        color: delma.containsMouse ? Qt.rgba(
                                                                         0.94,
                                                                         0.33,
                                                                         0.31,
                                                                         0.25) : "transparent"
                                        border.color: "#EF5350"
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
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        MouseArea {
                                            id: delma
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (cppTimeline
                                                        && effectsDialog.clipId >= 0) {
                                                    cppTimeline.removeEffect(
                                                                effectsDialog.clipId,
                                                                modelData.id)
                                                    effectsDialog.refreshEffects()
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    height: 1
                                    color: Theme.borderLight
                                    opacity: 0.4
                                    visible: index < effectsDialog.appliedEffects.length - 1
                                }
                                MouseArea {
                                    id: rh
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
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
                height: 56
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
                        enabled: effectsDialog.appliedEffects.length > 0
                        Layout.preferredHeight: 34
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
                                                     0.94, 0.33, 0.31,
                                                     0.25) : parent.hovered ? Qt.rgba(0.94, 0.33, 0.31, 0.12) : "transparent"
                            border.color: "#EF5350"
                            border.width: 1
                            opacity: parent.enabled ? 1.0 : 0.4
                        }
                        onClicked: {
                            if (cppTimeline && effectsDialog.clipId >= 0) {
                                cppTimeline.removeEffect(effectsDialog.clipId,
                                                         "brightness")
                                cppTimeline.removeEffect(effectsDialog.clipId,
                                                         "contrast")
                                cppTimeline.removeEffect(effectsDialog.clipId,
                                                         "saturation")
                                cppTimeline.removeEffect(effectsDialog.clipId,
                                                         "grayscale")
                                effectsDialog.refreshEffects()
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        text: "Закрыть"
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 90
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
                            color: parent.down ? Theme.rubyDark : parent.hovered ? Theme.rubyLight : Theme.rubyPrimary
                            Behavior on color {
                                ColorAnimation {
                                    duration: 100
                                }
                            }
                        }
                        onClicked: effectsDialog.closeDialog()
                    }
                }
            }
        }
    }
}
