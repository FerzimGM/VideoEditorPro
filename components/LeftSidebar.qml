import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground

    property int currentMode: 0 // 0 - эффекты, 1 - экспорт

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

        // Режим эффектов
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentMode === 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: Theme.spacing

                // Заголовок
                Text {
                    text: "ПАНЕЛЬ ИНСТРУМЕНТОВ"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    Layout.fillWidth: true
                }

                // НАСТРОЙКИ ЭФФЕКТА - 1/3 СВЕРХУ
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
                                    implicitWidth: 6
                                    radius: 3
                                    color: Theme.rubyPrimary
                                    opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                                }
                            }

                            Text {
                                text: "Выберите эффект\nдля настройки"
                                color: Theme.textDisabled
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                // ЭФФЕКТЫ - 2/3 СНИЗУ
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
                                implicitWidth: 6
                                radius: 3
                                color: Theme.rubyPrimary
                                opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                            }
                        }

                        Column {
                            width: parent.parent.width - 20
                            spacing: 0

                            EffectCategory {
                                title: "Видео эффекты"
                                effects: [
                                    { name: "Яркость/Контраст", icon: "☀" },
                                    { name: "Насыщенность", icon: "🎨" },
                                    { name: "Размытие", icon: "◎" },
                                    { name: "Резкость", icon: "⬥" },
                                    { name: "Цветокоррекция", icon: "🎭" }
                                ]
                            }

                            EffectCategory {
                                title: "Переходы"
                                effects: [
                                    { name: "Растворение", icon: "⊶" },
                                    { name: "Затемнение", icon: "⬛" },
                                    { name: "Вытеснение", icon: "➤" },
                                    { name: "Масштабирование", icon: "⊕" },
                                    { name: "Скольжение", icon: "⇒" },
                                    { name: "Zoom", icon: "⊙" }
                                ]
                            }

                            EffectCategory {
                                title: "Аудио эффекты"
                                effects: [
                                    { name: "Громкость", icon: "🔊" },
                                    { name: "Эквалайзер", icon: "📊" },
                                    { name: "Реверберация", icon: "〰" },
                                    { name: "Шумоподавление", icon: "🔇" },
                                    { name: "Компрессор", icon: "⊡" },
                                    { name: "Эхо", icon: "↷" }
                                ]
                            }
                        }
                    }
                }
            }
        }

        // Режим экспорта
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentMode === 1

            ExportPanel {
                anchors.fill: parent
                anchors.margins: Theme.spacing
            }
        }
    }

    component ExportPanel: ColumnLayout {
        spacing: Theme.spacingLarge

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

        Button {
            text: "СОХРАНИТЬ ВИДЕО"
            Layout.fillWidth: true
            Layout.preferredHeight: 50

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
                Behavior on color { ColorAnimation { duration: Theme.animationDuration } }
            }
        }

        Item { Layout.fillHeight: true }
    }

    component EffectCategory: Column {
        property string title: ""
        property var effects: []
        property bool collapsed: false

        width: parent.width
        spacing: 0

        // Заголовок категории - кликабельный
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

                Text {
                    text: collapsed ? "▶" : "▼"
                    color: Theme.rubyPrimary
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                }

                Text {
                    text: title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                    Layout.fillWidth: true
                }
            }

            MouseArea {
                id: headerMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: parent.parent.collapsed = !parent.parent.collapsed
            }
        }

        // Эффекты в категории - СДВИГАЮТСЯ при сворачивании
        Column {
            width: parent.width
            spacing: Theme.spacingSmall
            visible: !parent.collapsed
            height: visible ? implicitHeight : 0

            Behavior on height {
                NumberAnimation { duration: Theme.animationDuration }
            }

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

                        Text {
                            text: modelData.icon
                            color: Theme.rubyLight
                            font.pixelSize: 16
                        }

                        Text {
                            text: modelData.name
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: effectMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: console.log("Эффект:", modelData.name)
                    }
                }
            }
        }
    }

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
