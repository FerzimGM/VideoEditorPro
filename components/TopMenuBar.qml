import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.panelBackground
    
    signal openVideo()
    signal openProject()
    signal saveProject()
    signal exportVideo()  // Новый сигнал!
    signal minimize()
    signal maximize()
    signal close()
    
    // Ссылка на Window
    property var targetWindow: null

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: Theme.spacing

        // Кнопка меню
        CustomButton {
            text: "Меню"
            icon: "☰"
            onClicked: {
                if (mainMenu.visible) {
                    mainMenu.close()
                } else {
                    mainMenu.popup(this, 0, height)
                }
            }
        }

        Text {
            text: "VideoEditor Pro"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }

        // Пустая область для перетаскивания окна
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            MouseArea {
                anchors.fill: parent
                
                property point clickPos: Qt.point(0, 0)
                
                onPressed: (mouse) => {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }
                
                onPositionChanged: (mouse) => {
                    if (pressed && root.targetWindow) {
                        var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                        root.targetWindow.x += delta.x
                        root.targetWindow.y += delta.y
                    }
                }
                
                onDoubleClicked: {
                    if (root.targetWindow) {
                        root.maximize()
                    }
                }
            }
        }

        // Кнопки управления окном
        RowLayout {
            Layout.fillHeight: true
            spacing: 0

            CustomButton {
                text: "−"
                onClicked: root.minimize()
            }

            CustomButton {
                text: "□"
                onClicked: root.maximize()
            }

            CustomButton {
                text: "×"
                onClicked: root.close()
            }
        }
    }

    // Меню
    Menu {
        id: mainMenu
        width: 250
        
        background: Rectangle {
            color: Theme.panelBackground
            border.color: Theme.rubyPrimary
            border.width: 1
            radius: Theme.borderRadius
        }
        
        delegate: MenuItem {
            id: menuItem
            implicitWidth: parent ? parent.width : 0
            implicitHeight: 35
            
            contentItem: RowLayout {
                spacing: Theme.spacingLarge
                
                Text {
                    text: menuItem.text
                    color: menuItem.highlighted ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.fillWidth: true
                }
                
                Text {
                    text: menuItem.action && menuItem.action.shortcut ? menuItem.action.shortcut : ""
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    visible: text !== ""
                }
            }
            
            background: Rectangle {
                color: menuItem.highlighted ? Theme.hoverColor : "transparent"
                radius: Theme.borderRadius
            }
        }
        
        Action {
            text: "Открыть видео"
            shortcut: "Ctrl+O"
            onTriggered: root.openVideo()
        }
        
        Action {
            text: "Открыть проект"
            shortcut: "Ctrl+Shift+O"
            onTriggered: root.openProject()
        }
        
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }
        
        Action {
            text: "Сохранить проект"
            shortcut: "Ctrl+S"
            onTriggered: root.saveProject()
        }
        
        Action {
            text: "Сохранить как..."
            shortcut: "Ctrl+Shift+S"
            onTriggered: root.saveProject()
        }
        
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }
        
        Action {
            text: "Экспорт видео..."
            shortcut: "Ctrl+E"
            onTriggered: root.exportVideo()
        }
        
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }
        
        Action {
            text: "Выход"
            shortcut: "Alt+F4"
            onTriggered: root.close()
        }
    }

    component CustomButton: Rectangle {
        property string text: ""
        property string icon: ""
        signal clicked()
        
        implicitWidth: contentRow.width + 16
        implicitHeight: 30
        color: mouseArea.containsMouse ? Theme.buttonHover : "transparent"
        radius: Theme.borderRadius
        
        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: Theme.spacingSmall
            
            Text {
                text: icon
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLarge
                visible: icon !== ""
            }
            
            Text {
                text: parent.parent.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
        }
        
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    component WindowButton: Rectangle {
        property string text: ""
        property string tooltip: ""
        property bool isClose: false
        signal clicked()
        
        implicitWidth: 40
        implicitHeight: parent.height
        color: {
            if (mouseArea.pressed) return isClose ? "#c0392b" : Theme.buttonPressed
            if (mouseArea.containsMouse) return isClose ? Theme.rubyPrimary : Theme.buttonHover
            return "transparent"
        }
        
        Text {
            anchors.centerIn: parent
            text: parent.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.bold: isClose
        }
        
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            z: 1000
            onClicked: {
                console.log("WindowButton кликнута:", parent.text)
                parent.clicked()
            }
        }
        
        ToolTip {
            visible: mouseArea.containsMouse
            text: tooltip
            delay: 500
            
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
    }
}
