import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

// ===== ПАНЕЛЬ ЭФФЕКТОВ КЛИПА =====
// Вызывается из VideoClip.qml через onEffectsRequested.
// Подключение в main.qml:
//
//   ClipEffectsDialog {
//       id: clipEffectsDialog
//       parentWindow: root  // ← Window
//   }
//
// И в Timeline → Track → VideoClip:
//   onEffectsRequested: id => clipEffectsDialog.openForClip(id)

Window {
    id: effectsDialog
    title: "Эффекты клипа"
    width: 480
    height: 520
    minimumWidth: 380
    minimumHeight: 400
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal   // не блокирует главное окно

    // ===== СВОЙСТВА =====
    property int clipId: -1
    property string clipName: ""
    property var parentWindow: null

    // Список применённых эффектов (заготовка — в будущем из C++)
    property var appliedEffects: []

    // ===== API =====
    function openForClip(id) {
        clipId = id
        // Получаем имя клипа из C++ если доступно
        if (cppTimeline) {
            var clips1 = cppTimeline.getClipsForTrack(1)
            var clips2 = cppTimeline.getClipsForTrack(2)
            var allClips = clips1.concat(clips2)
            for (var i = 0; i < allClips.length; i++) {
                if (allClips[i].id === id) {
                    clipName = allClips[i].filename || ("Клип #" + id)
                    break
                }
            }
        } else {
            clipName = "Клип #" + id
        }

        // Центрируем относительно родительского окна
        if (parentWindow) {
            x = parentWindow.x + (parentWindow.width  - width)  / 2
            y = parentWindow.y + (parentWindow.height - height) / 2
        }

        visible = true
        raise()
    }

    function closeDialog() {
        visible = false
        clipId = -1
        clipName = ""
        appliedEffects = []
    }

    // ===== UI =====
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
        radius: Theme.borderRadius
        border.color: Theme.rubyPrimary
        border.width: 2

        // Тень
        layer.enabled: true
        layer.effect: null

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── Заголовок ──
            Rectangle {
                Layout.fillWidth: true
                height: 44
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                // Нижний край без скруглений
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.bottom: parent.bottom; height: Theme.borderRadius
                    color: parent.color
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    anchors.rightMargin: Theme.spacing
                    spacing: Theme.spacing

                    // Иконка
                    Text { text: "✨"; font.pixelSize: 18 }

                    // Заголовок
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "Эффекты"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLarge
                            font.bold: true
                        }
                        Text {
                            text: effectsDialog.clipName || "—"
                            color: Theme.rubyLight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    // Drag handle для перемещения окна
                    MouseArea {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        property real sx: 0; property real sy: 0
                        cursorShape: Qt.SizeAllCursor
                        onPressed: { sx = mouseX; sy = mouseY }
                        onPositionChanged: {
                            if (pressed) {
                                effectsDialog.x += mouseX - sx
                                effectsDialog.y += mouseY - sy
                            }
                        }
                    }

                    // Кнопка закрыть
                    Rectangle {
                        width: 28; height: 28; radius: 14
                        color: closeBtn.containsMouse ? Theme.rubyPrimary : "transparent"
                        border.color: Theme.rubyPrimary; border.width: 1
                        Text {
                            anchors.centerIn: parent; text: "✕"
                            color: Theme.textPrimary; font.pixelSize: 13; font.bold: true
                        }
                        MouseArea {
                            id: closeBtn; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: effectsDialog.closeDialog()
                        }
                    }
                }
            }

            // ── Контент ──
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLarge
                    spacing: Theme.spacingLarge

                    // Информация о клипе
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        color: Theme.backgroundDark
                        radius: Theme.borderRadius
                        border.color: Theme.borderLight; border.width: 1
                        RowLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacing; spacing: Theme.spacing
                            Text { text: "🎬"; font.pixelSize: 16 }
                            Text {
                                text: "ID: " + (effectsDialog.clipId >= 0 ? effectsDialog.clipId : "—")
                                color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: effectsDialog.appliedEffects.length + " эффект(ов)"
                                color: effectsDialog.appliedEffects.length > 0 ? Theme.rubyLight : Theme.textDisabled
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                            }
                        }
                    }

                    // Заголовок списка
                    Text {
                        text: "ПРИМЕНЁННЫЕ ЭФФЕКТЫ"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }

                    // Список эффектов
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: Theme.backgroundDark
                        radius: Theme.borderRadius
                        border.color: Theme.borderLight; border.width: 1
                        clip: true

                        // Плейсхолдер если нет эффектов
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: Theme.spacing
                            visible: effectsDialog.appliedEffects.length === 0
                            Text { text: "🎞️"; font.pixelSize: 36; Layout.alignment: Qt.AlignHCenter; opacity: 0.4 }
                            Text {
                                text: "Эффекты не применены"
                                color: Theme.textDisabled; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Выберите эффект в левой панели\nи нажмите «Применить»"
                                color: Theme.textDisabled; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                                Layout.alignment: Qt.AlignHCenter; horizontalAlignment: Text.AlignHCenter; opacity: 0.7
                            }
                        }

                        // Список применённых эффектов
                        ScrollView {
                            anchors.fill: parent; clip: true
                            visible: effectsDialog.appliedEffects.length > 0
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.rubyPrimary; opacity: 0.6 }
                            }
                            Column {
                                width: parent.width; spacing: 1; padding: 6
                                Repeater {
                                    model: effectsDialog.appliedEffects
                                    delegate: Rectangle {
                                        width: parent.width - 12; height: 44
                                        color: effectRowMA.containsMouse ? Theme.hoverColor : "transparent"
                                        radius: Theme.borderRadius

                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: Theme.spacing; spacing: Theme.spacing
                                            Text { text: modelData.icon || "⚙️"; font.pixelSize: 16 }
                                            ColumnLayout {
                                                Layout.fillWidth: true; spacing: 2
                                                Text { text: modelData.name; color: Theme.textPrimary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; font.bold: true }
                                                Text { text: modelData.params || ""; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                                            }
                                            // Кнопка удалить эффект
                                            Rectangle {
                                                width: 24; height: 24; radius: 4
                                                color: delEffectMA.containsMouse ? Qt.rgba(0.94, 0.33, 0.31, 0.2) : "transparent"
                                                border.color: "#EF5350"; border.width: 1
                                                Text { anchors.centerIn: parent; text: "✕"; color: "#EF5350"; font.pixelSize: 11; font.bold: true }
                                                MouseArea {
                                                    id: delEffectMA; anchors.fill: parent; hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        var arr = effectsDialog.appliedEffects.slice()
                                                        arr.splice(index, 1)
                                                        effectsDialog.appliedEffects = arr
                                                        console.log("🗑️ Удалён эффект:", modelData.name, "с клипа", effectsDialog.clipId)
                                                        // TODO: вызов C++ для удаления эффекта
                                                        // cppTimeline.removeEffect(effectsDialog.clipId, modelData.id)
                                                    }
                                                }
                                            }
                                        }
                                        MouseArea { id: effectRowMA; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                    }
                                }
                            }
                        }
                    }

                    // ── Кнопки ──
                    RowLayout {
                        Layout.fillWidth: true; spacing: Theme.spacing

                        // Сбросить все
                        Button {
                            text: "Сбросить все"
                            enabled: effectsDialog.appliedEffects.length > 0
                            Layout.preferredHeight: 38
                            contentItem: Text {
                                text: parent.text; color: enabled ? "#EF5350" : Theme.textDisabled
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.down ? Qt.rgba(0.94, 0.33, 0.31, 0.2) : (parent.hovered ? Qt.rgba(0.94, 0.33, 0.31, 0.1) : "transparent")
                                radius: Theme.borderRadius; border.color: "#EF5350"; border.width: 1
                            }
                            onClicked: {
                                effectsDialog.appliedEffects = []
                                console.log("🗑️ Сброшены все эффекты клипа", effectsDialog.clipId)
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Закрыть
                        Button {
                            text: "Закрыть"
                            Layout.preferredHeight: 38; Layout.preferredWidth: 100
                            contentItem: Text {
                                text: parent.text; color: Theme.textPrimary
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary)
                                radius: Theme.borderRadius
                            }
                            onClicked: effectsDialog.closeDialog()
                        }
                    }
                }
            }
        }
    }
}
