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
    property int clipId: -1
    property string thumbnailPath: ""  // Путь к превью кадру (от FFmpeg)
    
    signal moved(real newX)
    signal clicked()
    signal deleteRequested(int clipId)
    
    // ===== ПРЕВЬЮ КАДРА =====
    // TODO: FFmpeg will generate thumbnail
    //cppTimeline.generateThumbnail(clipId, 0.0) → signal thumbnailReady(clipId, path)
    Image {
        anchors.fill: parent
        anchors.margins: 2
        source: root.thumbnailPath ? "file:///" + root.thumbnailPath : ""
        fillMode: Image.PreserveAspectCrop
        visible: root.thumbnailPath !== ""
        opacity: 0.3  // Полупрозрачный чтобы видеть название
        
        // Placeholder пока нет FFmpeg
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0.2, 0.2, 0.3, 0.5)
            visible: root.thumbnailPath === ""
            
            Text {
                anchors.centerIn: parent
                text: "🎬"
                color: Theme.rubyPrimary
                font.pixelSize: 32
                opacity: 0.3
            }
        }
    }

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
        acceptedButtons: Qt.LeftButton | Qt.RightButton  // Левый и правый клик
        
        property real startX: 0
        property real dragStartX: 0
        
        onPressed: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                // Правый клик - показываем меню
                contextMenu.popup()
            } else {
                // Левый клик - начинаем перетаскивание
                startX = root.x
                dragStartX = mouse.x
                cursorShape = Qt.ClosedHandCursor
            }
        }
        
        onPositionChanged: (mouse) => {
            if (pressed && mouse.buttons & Qt.LeftButton) {
                var delta = mouse.x - dragStartX
                root.x = startX + delta
            }
        }
        
        onReleased: {
            cursorShape = Qt.OpenHandCursor
            if (pressed) {
                root.moved(root.x)
            }
        }
        
        onClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                root.clicked()
            }
        }
    }
    
    // Контекстное меню
    Menu {
        id: contextMenu
        
        MenuItem {
            text: "✂ Разрезать"
            onTriggered: {
                console.log("Разрезать клип:", root.clipId)
                // TODO: emit splitRequested(clipId, currentTime)
            }
        }
        
        MenuItem {
            text: "📋 Копировать"
            onTriggered: {
                console.log("Копировать клип:", root.clipId)
            }
        }
        
        MenuSeparator { }
        
        MenuItem {
            text: "🗑 Удалить"
            onTriggered: {
                console.log("Удалить клип:", root.clipId)
                root.deleteRequested(root.clipId)
            }
        }
        
        background: Rectangle {
            color: Theme.panelBackground
            border.color: Theme.rubyPrimary
            border.width: 1
            radius: Theme.borderRadius
        }
        
        delegate: MenuItem {
            id: menuItem
            
            contentItem: Text {
                text: menuItem.text
                color: menuItem.highlighted ? Theme.rubyPrimary : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            
            background: Rectangle {
                color: menuItem.highlighted ? Theme.hoverColor : "transparent"
                radius: Theme.borderRadius
            }
        }
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
