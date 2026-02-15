import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground

    property int currentMode: 0 // 0 - эффекты, 1 - экспорт
    property var videoPlayer: null  // Ссылка на VideoPlayer для эффектов
    property string selectedEffect: ""  // Выбранный эффект

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

                            ColumnLayout {
                                width: parent.width - 20
                                spacing: Theme.spacing
                                
                                // Заголовок выбранного эффекта
                                Text {
                                    text: root.selectedEffect || "Выберите эффект"
                                    color: root.selectedEffect ? Theme.rubyPrimary : Theme.textDisabled
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLarge
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                                
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Theme.dividerColor
                                    visible: root.selectedEffect !== ""
                                }
                                
                                // ===== ПАРАМЕТРЫ ЯРКОСТИ =====
                                Column {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing
                                    visible: root.selectedEffect === "Яркость"
                                    
                                    Text {
                                        text: "Уровень яркости"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                    }
                                    
                                    Slider {
                                        width: parent.width
                                        from: 0.5
                                        to: 2.0
                                        value: 1.0
                                        stepSize: 0.1
                                        
                                        onMoved: {
                                            if (root.videoPlayer) {
                                                root.videoPlayer.brightness = value
                                            }
                                        }
                                        
                                        background: Rectangle {
                                            x: parent.leftPadding
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
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
                                            x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width / 2
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                            width: 16
                                            height: 16
                                            radius: 8
                                            color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary
                                        }
                                    }
                                    
                                    Text {
                                        text: root.videoPlayer ? root.videoPlayer.brightness.toFixed(2) + "x" : "1.00x"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                                
                                // ===== ПАРАМЕТРЫ КОНТРАСТА =====
                                Column {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing
                                    visible: root.selectedEffect === "Контраст"
                                    
                                    Text {
                                        text: "Уровень контраста"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                    }
                                    
                                    Slider {
                                        width: parent.width
                                        from: 0.5
                                        to: 2.0
                                        value: 1.0
                                        stepSize: 0.1
                                        
                                        onMoved: {
                                            if (root.videoPlayer) {
                                                root.videoPlayer.contrast = value
                                            }
                                        }
                                        
                                        background: Rectangle {
                                            x: parent.leftPadding
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
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
                                            x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width / 2
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                            width: 16
                                            height: 16
                                            radius: 8
                                            color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary
                                        }
                                    }
                                    
                                    Text {
                                        text: root.videoPlayer ? root.videoPlayer.contrast.toFixed(2) + "x" : "1.00x"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                                
                                // ===== ПАРАМЕТРЫ НАСЫЩЕННОСТИ =====
                                Column {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing
                                    visible: root.selectedEffect === "Насыщенность"
                                    
                                    Text {
                                        text: "Уровень насыщенности"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                    }
                                    
                                    Slider {
                                        width: parent.width
                                        from: 0.0
                                        to: 2.0
                                        value: 1.0
                                        stepSize: 0.1
                                        
                                        onMoved: {
                                            if (root.videoPlayer) {
                                                root.videoPlayer.saturation = value
                                            }
                                        }
                                        
                                        background: Rectangle {
                                            x: parent.leftPadding
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
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
                                            x: parent.leftPadding + parent.visualPosition * parent.availableWidth - width / 2
                                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                            width: 16
                                            height: 16
                                            radius: 8
                                            color: parent.pressed ? Theme.rubyLight : Theme.rubyPrimary
                                        }
                                    }
                                    
                                    Text {
                                        text: root.videoPlayer ? root.videoPlayer.saturation.toFixed(2) + "x" : "1.00x"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                    
                                    CheckBox {
                                        text: "Чёрно-белое"
                                        
                                        onToggled: {
                                            if (root.videoPlayer) {
                                                root.videoPlayer.grayscale = checked
                                            }
                                        }
                                        
                                        contentItem: Text {
                                            text: parent.text
                                            color: Theme.textPrimary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSize
                                            leftPadding: parent.indicator.width + parent.spacing
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        
                                        indicator: Rectangle {
                                            width: 20
                                            height: 20
                                            radius: 3
                                            color: parent.checked ? Theme.rubyPrimary : Theme.backgroundDark
                                            border.color: Theme.rubyPrimary
                                            border.width: 2
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: "✓"
                                                color: "white"
                                                font.pixelSize: 16
                                                font.bold: true
                                                visible: parent.parent.checked
                                            }
                                        }
                                    }
                                }
                                
                                // TODO: Добавить параметры для остальных эффектов
                                Text {
                                    visible: root.selectedEffect !== "" && 
                                            root.selectedEffect !== "Яркость" && 
                                            root.selectedEffect !== "Контраст" && 
                                            root.selectedEffect !== "Насыщенность"
                                    text: "Параметры для эффекта\n\"" + root.selectedEffect + "\"\nбудут доступны в C++ версии"
                                    color: Theme.textDisabled
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                                
                                Item { Layout.fillHeight: true }
                                
                                // Кнопка сброса (всегда видна если эффект выбран)
                                Button {
                                    visible: root.selectedEffect !== ""
                                    text: "Сбросить эффекты"
                                    Layout.fillWidth: true
                                    
                                    onClicked: {
                                        if (root.videoPlayer) {
                                            root.videoPlayer.resetEffects()
                                        }
                                    }
                                    
                                    contentItem: Text {
                                        text: parent.text
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    
                                    background: Rectangle {
                                        color: parent.down ? Theme.rubyDark : (parent.hovered ? Theme.rubyLight : Theme.rubyPrimary)
                                        radius: Theme.borderRadius
                                    }
                                }
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

        Text {
            text: label
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            Layout.fillWidth: true
        }
    }
}
