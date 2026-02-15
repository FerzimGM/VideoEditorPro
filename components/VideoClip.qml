import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: selected ? Theme.clipSelectedColor : Theme.clipColor
    radius: Theme.borderRadius
    border.color: selected ? Theme.rubyPrimary : Theme.borderLight
    border.width: selected ? 2 : 1

    property string clipName: "Clip"
    property bool selected: false
    
    signal moved(real newX)
    signal clicked()

    // Градиент для красоты
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(root.color, 1.1) }
        GradientStop { position: 1.0; color: root.color }
    }

    // Название клипа
    Text {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        text: root.clipName
        color: "#FFFFFF"
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
    }

    // Перетаскивание
    MouseArea {
        id: dragArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor
        
        property real startX: 0
        property real dragStartX: 0
        
        onPressed: (mouse) => {
            startX = root.x
            dragStartX = mouse.x
            cursorShape = Qt.ClosedHandCursor
        }
        
        onPositionChanged: (mouse) => {
            if (pressed) {
                var delta = mouse.x - dragStartX
                root.x = startX + delta
            }
        }
        
        onReleased: {
            cursorShape = Qt.OpenHandCursor
            root.moved(root.x)
        }
        
        onClicked: root.clicked()
    }

    // Resize handle слева
    Rectangle {
        id: leftHandle
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        color: "transparent"
        
        MouseArea {
            id: leftMouseArea
            anchors.fill: parent
            cursorShape: Qt.SizeHorCursor
            hoverEnabled: true
            
            property real startX: 0
            property real startWidth: 0
            
            onPressed: (mouse) => {
                startX = mouse.x
                startWidth = root.width
            }
            
            onPositionChanged: (mouse) => {
                if (pressed) {
                    var delta = mouse.x - startX
                    var newWidth = startWidth - delta
                    if (newWidth >= 20) {
                        root.x += delta
                        root.width = newWidth
                    }
                }
            }
        }
        
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height
            color: Theme.rubyLight
            opacity: leftMouseArea.containsMouse ? 0.8 : 0
        }
    }

    // Resize handle справа
    Rectangle {
        id: rightHandle
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        color: "transparent"
        
        MouseArea {
            id: rightMouseArea
            anchors.fill: parent
            cursorShape: Qt.SizeHorCursor
            hoverEnabled: true
            
            property real startX: 0
            property real startWidth: 0
            
            onPressed: (mouse) => {
                startX = mouse.x
                startWidth = root.width
            }
            
            onPositionChanged: (mouse) => {
                if (pressed) {
                    var delta = mouse.x - startX
                    var newWidth = startWidth + delta
                    if (newWidth >= 20) {
                        root.width = newWidth
                    }
                }
            }
        }
        
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height
            color: Theme.rubyLight
            opacity: rightMouseArea.containsMouse ? 0.8 : 0
        }
    }

    // Анимация выделения
    Behavior on color {
        ColorAnimation { duration: Theme.animationDuration }
    }
    
    Behavior on border.width {
        NumberAnimation { duration: Theme.animationDuration }
    }
}
