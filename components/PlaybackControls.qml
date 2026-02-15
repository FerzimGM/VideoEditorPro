import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground

    property bool isPlaying: false
    property real currentTime: 0
    property real duration: 100
    property real playbackSpeed: 1.0
    property bool snapEnabled: true

    signal playPauseClicked()
    signal stopClicked()
    signal seek(real time)
    signal speedChanged(real speed)
    signal snapToggled()
    signal cutClicked()
    signal clearEffectsClicked()

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        // Кнопки воспроизведения
        RowLayout {
            spacing: Theme.spacingSmall

            ControlButton {
                icon: "⏮"
                tooltip: "Назад на " + (5 * root.playbackSpeed).toFixed(1) + " сек"
                size: 38
                onClicked: {
                    var rewindAmount = 5 * root.playbackSpeed  // 5 сек × скорость
                    root.seek(Math.max(0, root.currentTime - rewindAmount))
                }
            }

            ControlButton {
                icon: root.isPlaying ? "⏸" : "▶"
                tooltip: root.isPlaying ? "Пауза (K)" : "Воспроизведение (K)"
                highlighted: true
                size: 46
                onClicked: root.playPauseClicked()
            }

            ControlButton {
                icon: "⏹"
                tooltip: "Стоп"
                size: 38
                onClicked: root.stopClicked()
            }

            ControlButton {
                icon: "⏭"
                tooltip: "Вперёд на " + (5 * root.playbackSpeed).toFixed(1) + " сек"
                size: 38
                onClicked: {
                    var fastForwardAmount = 5 * root.playbackSpeed  // 5 сек × скорость
                    root.seek(Math.min(root.duration, root.currentTime + fastForwardAmount))
                }
            }
        }

        Rectangle {
            width: 2
            Layout.fillHeight: true
            Layout.topMargin: Theme.spacing
            Layout.bottomMargin: Theme.spacing
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.rubyGradientStart }
                GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
            }
        }

        // Инструменты редактирования
        RowLayout {
            spacing: Theme.spacingSmall

            ControlButton {
                icon: "✂"
                tooltip: "Разрезать клип (C)"
                size: 38
                onClicked: root.cutClicked()
            }

            ControlButton {
                icon: "🗑"
                tooltip: "Удалить эффекты"
                size: 38
                onClicked: root.clearEffectsClicked()
            }
        }

        Rectangle {
            width: 2
            Layout.fillHeight: true
            Layout.topMargin: Theme.spacing
            Layout.bottomMargin: Theme.spacing
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.rubyGradientStart }
                GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
            }
        }

        // Скорость воспроизведения
        RowLayout {
            spacing: Theme.spacing

            Text {
                text: "Скорость:"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                font.bold: true
            }

            ComboBox {
                id: speedCombo
                implicitWidth: 100
                model: ["0.25x", "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"]
                currentIndex: 3

                contentItem: Text {
                    text: speedCombo.displayText
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacing
                }

                background: Rectangle {
                    color: speedCombo.down ? Theme.buttonPressed : (speedCombo.hovered ? Theme.buttonHover : Theme.buttonBackground)
                    radius: Theme.borderRadius
                    border.color: Theme.rubyPrimary
                    border.width: 1
                }

                delegate: ItemDelegate {
                    width: speedCombo.width
                    
                    contentItem: Text {
                        text: modelData
                        color: highlighted ? Theme.rubyLight : Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacing
                    }

                    background: Rectangle {
                        color: highlighted ? Theme.hoverColor : "transparent"
                        radius: Theme.borderRadius
                    }
                }

                popup: Popup {
                    y: speedCombo.height
                    width: speedCombo.width
                    padding: 4

                    background: Rectangle {
                        color: Theme.panelBackground
                        border.color: Theme.rubyPrimary
                        border.width: 1
                        radius: Theme.borderRadius
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: speedCombo.delegateModel
                    }
                }

                onActivated: {
                    var speeds = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0]
                    root.speedChanged(speeds[currentIndex])
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Дополнительные настройки
        RowLayout {
            spacing: Theme.spacing

            ControlButton {
                icon: "🧲"
                tooltip: "Магнит к краям клипов"
                checkable: true
                checked: root.snapEnabled
                size: 38
                onClicked: root.snapToggled()
            }

            Rectangle {
                width: timecodeDisplay.width + Theme.spacing * 2
                height: 32
                color: Theme.backgroundDark
                radius: Theme.borderRadius
                border.color: Theme.rubyPrimary
                border.width: 2

                Text {
                    id: timecodeDisplay
                    anchors.centerIn: parent
                    text: formatTime(root.currentTime)
                    color: Theme.rubyLight
                    font.family: "Consolas, monospace"
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                }
            }
        }
    }

    component ControlButton: Rectangle {
        property string icon: ""
        property string tooltip: ""
        property bool highlighted: false
        property bool checkable: false
        property bool checked: false
        property int size: 38
        signal clicked()

        implicitWidth: size
        implicitHeight: size
        radius: size / 2

        color: {
            if (mouseArea.pressed) return highlighted ? Theme.rubyDark : Theme.buttonPressed
            if (mouseArea.containsMouse) return highlighted ? Theme.rubyLight : Theme.hoverColor
            if (checked) return Theme.rubyPrimary
            if (highlighted) return Theme.rubyPrimary
            return Theme.buttonBackground
        }

        border.color: (highlighted || checked) ? "transparent" : (mouseArea.containsMouse ? Theme.rubyPrimary : Theme.borderLight)
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: icon
            color: (highlighted || checked) ? "#FFFFFF" : (mouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary)
            font.pixelSize: size * 0.45
            font.bold: highlighted || checked || mouseArea.containsMouse
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if (checkable) {
                    parent.checked = !parent.checked
                }
                parent.clicked()
            }
        }

        ToolTip {
            visible: mouseArea.containsMouse
            text: tooltip
            delay: 300

            contentItem: Text {
                text: parent.text || ""
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            background: Rectangle {
                color: Theme.panelBackground
                border.color: Theme.rubyPrimary
                border.width: 1
                radius: Theme.borderRadius
            }
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animationDuration }
        }

        scale: mouseArea.pressed ? 0.9 : 1.0
        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
    }

    function formatTime(seconds) {
        var hours = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = Math.floor(seconds % 60)
        
        if (hours > 0) {
            return pad(hours) + ":" + pad(mins) + ":" + pad(secs)
        }
        return pad(mins) + ":" + pad(secs)
    }

    function pad(num) {
        return num < 10 ? "0" + num : num
    }
}
