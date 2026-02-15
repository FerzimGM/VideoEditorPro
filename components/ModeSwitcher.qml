import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground
    radius: Theme.borderRadius
    border.color: Theme.borderLight
    border.width: 1

    property int currentMode: 0
    signal modeChanged(int mode)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        Text {
            text: "РЕЖИМЫ"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.rubyGradientStart }
                GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
            }
        }

        ModeButton {
            text: "РЕДАКТИРОВАНИЕ"
            icon: "⬚"
            isSelected: root.currentMode === 0
            onClicked: {
                root.currentMode = 0
                root.modeChanged(0)
            }
        }

        ModeButton {
            text: "ЭКСПОРТ"
            icon: "💾"
            isSelected: root.currentMode === 1
            onClicked: {
                root.currentMode = 1
                root.modeChanged(1)
            }
        }

        Item { Layout.fillHeight: true }
    }

    component ModeButton: Rectangle {
        property string text: ""
        property string icon: ""
        property bool isSelected: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 100
        
        color: {
            if (isSelected) return Theme.selectedColor
            if (mouseArea.containsMouse) return Theme.hoverColor
            return Theme.buttonBackground
        }
        
        radius: Theme.borderRadius
        border.color: isSelected ? Theme.rubyPrimary : Theme.borderLight
        border.width: isSelected ? 2 : 1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: isSelected ? Qt.rgba(0.78, 0.15, 0.31, 0.2) : "transparent" }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacing

            Text {
                text: icon
                color: isSelected ? Theme.rubyLight : Theme.textSecondary
                font.pixelSize: 32
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: parent.parent.text
                color: isSelected ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                font.bold: isSelected
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animationDuration }
        }
    }
}
