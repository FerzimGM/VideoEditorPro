import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

// VideoClip — клип на таймлайне, разбитый на видео и аудио полосы.
// СТРУКТУРА:
//   Item (root)
//   └── Column
//       ├── Rectangle (синий)  — видео дорожка (50px)
//       │   ├── Label "V" + имя файла
//       │   ├── Resize handles (слева/справа)
//       │   └── MouseArea (drag + контекстное меню)
//       └── Rectangle (зелёный) — аудио дорожка (30px)
//           ├── Label "A" + псевдо-осциллограмма
//           └── MouseArea (mute + контекстное меню)
Item {
    id: root
    // ===== СВОЙСТВА =====
    property string clipName: "Clip"
    property int clipId: -1
    property bool isMuted: false

    //  selected привязан к selectionManager (объявлен в main.qml)
    // selectionManager.selectedClipId === clipId → этот клип выделен ,,,
    readonly property bool selected: selectionManager.selectedClipId === clipId

    // Высоты полос — должны совпадать с тем, что ожидает Track.qml
    readonly property real videoH: 50
    readonly property real audioH: 30

    height: videoH + audioH + 2 // 2px — зазор между полосами

    // ===== СИГНАЛЫ =====
    signal moved(real newX)
    signal clicked
    signal deleteRequested(int clipId)
    signal splitRequested(int clipId)
    signal effectsRequested(int clipId)
    signal muteToggled(int clipId, bool muted)

    // Drag state (shared между видео и аудио MouseArea)
    property real _startX: 0
    property real _dragOfsX: 0
    property bool _dragging: false

    // ===== COLUMN: ВИДЕО + АУДИО =====
    Column {
        anchors.fill: parent
        spacing: 2

        // ─────────────────────────────────────────────
        // ВИДЕО ПОЛОСА (синяя)
        // ─────────────────────────────────────────────
        Rectangle {
            id: videoStrip
            width: parent.width
            height: root.videoH
            radius: Theme.borderRadius

            // Цвет: синий, выделенный — ярче
            color: root.selected ? "#1E88E5" : "#1565C0"
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: root.selected ? "#42A5F5" : "#1E88E5"
                }
                GradientStop {
                    position: 1.0
                    color: root.selected ? "#1E88E5" : "#1565C0"
                }
            }
            border.color: root.selected ? Theme.rubyPrimary : "#0D47A1"
            border.width: root.selected ? 2 : 1

            // Метка "V"
            Rectangle {
                id: vLabel
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    margins: 2
                }
                width: 20
                radius: Theme.borderRadius
                color: Qt.rgba(0, 0, 0, 0.3)
                Text {
                    anchors.centerIn: parent
                    text: "V"
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // Имя файла
            Text {
                anchors {
                    left: vLabel.right
                    right: rightHandle.left
                    verticalCenter: parent.verticalCenter
                }
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                text: root.clipName
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                elide: Text.ElideRight
            }

            // Левый resize handle
            Item {
                id: leftHandle
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 8
                z: 10

                Rectangle {
                    anchors.centerIn: parent
                    width: 3
                    height: parent.height * 0.55
                    radius: 2
                    color: "white"
                    opacity: leftMA.containsMouse ? 0.9 : 0.35
                }

                MouseArea {
                    id: leftMA
                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true

                    property real _sx: 0
                    property real _sw: 0

                    onPressed: {
                        _sx = mouse.x
                        _sw = root.width
                    }
                    onPositionChanged: {
                        if (pressed) {
                            var d = mouse.x - _sx
                            var nw = _sw - d
                            if (nw >= 30) {
                                root.x += d
                                root.width = nw
                            }
                        }
                    }
                    onReleased: root.moved(root.x)
                }
            }

            // Правый resize handle
            Item {
                id: rightHandle
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 8
                z: 10

                Rectangle {
                    anchors.centerIn: parent
                    width: 3
                    height: parent.height * 0.55
                    radius: 2
                    color: "white"
                    opacity: rightMA.containsMouse ? 0.9 : 0.35
                }

                MouseArea {
                    id: rightMA
                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true

                    property real _sx: 0
                    property real _sw: 0

                    onPressed: {
                        _sx = mouse.x
                        _sw = root.width
                    }
                    onPositionChanged: {
                        if (pressed) {
                            var nw = _sw + (mouse.x - _sx)
                            if (nw >= 30)
                                root.width = nw
                        }
                    }
                    onReleased: root.moved(root.x)
                }
            }

            // Drag + контекстное меню
            MouseArea {
                anchors {
                    fill: parent
                    leftMargin: 8
                    rightMargin: 8
                }
                hoverEnabled: true
                cursorShape: root._dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onPressed: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        root.clicked()
                        videoMenu.popup()
                    } else {
                        root._startX = root.x
                        root._dragOfsX = mouse.x
                        root._dragging = true
                        root.clicked()
                    }
                }
                onPositionChanged: function (mouse) {
                    if (root._dragging && (mouse.buttons & Qt.LeftButton))
                        root.x = root._startX + (mouse.x - root._dragOfsX)
                }
                onReleased: {
                    if (root._dragging) {
                        root._dragging = false
                        root.moved(root.x)
                    }
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }

        // ─────────────────────────────────────────────
        // АУДИО ПОЛОСА (зелёная)
        // ─────────────────────────────────────────────
        Rectangle {
            id: audioStrip
            width: parent.width
            height: root.audioH
            radius: Theme.borderRadius

            // Цвет: зелёный или серый (muted)
            color: root.isMuted ? "#555" : (root.selected ? "#43A047" : "#2E7D32")
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: root.isMuted ? "#666" : (root.selected ? "#66BB6A" : "#43A047")
                }
                GradientStop {
                    position: 1.0
                    color: root.isMuted ? "#444" : (root.selected ? "#43A047" : "#2E7D32")
                }
            }
            border.color: root.selected ? Theme.rubyPrimary : Qt.darker(color,
                                                                        1.4)
            border.width: root.selected ? 2 : 1

            // Метка "A" / 🔇
            Rectangle {
                id: aLabel
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    margins: 2
                }
                width: 20
                radius: Theme.borderRadius
                color: Qt.rgba(0, 0, 0, 0.3)
                Text {
                    anchors.centerIn: parent
                    text: root.isMuted ? "🔇" : "A"
                    color: "white"
                    font.pixelSize: root.isMuted ? 9 : 11
                    font.bold: true
                }
            }

            // Псевдо-осциллограмма (статический декор)
            Row {
                anchors {
                    left: aLabel.right
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 2
                clip: true

                Repeater {
                    // Количество баров зависит от ширины
                    model: Math.max(0, Math.floor((audioStrip.width - 30) / 4))
                    delegate: Rectangle {
                        width: 2
                        // Синус-волна → похоже на форму звуковой волны
                        height: Math.abs(
                                    Math.sin(
                                        index * 0.63 + 0.5)) * (audioStrip.height * 0.65) + 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(1, 1, 1, root.isMuted ? 0.15 : 0.35)
                        radius: 1
                    }
                }
            }

            // Drag + контекстное меню аудио
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root._dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onPressed: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        root.clicked()
                        audioMenu.popup()
                    } else {
                        root._startX = root.x
                        root._dragOfsX = mouse.x
                        root._dragging = true
                        root.clicked()
                    }
                }
                onPositionChanged: function (mouse) {
                    if (root._dragging && (mouse.buttons & Qt.LeftButton))
                        root.x = root._startX + (mouse.x - root._dragOfsX)
                }
                onReleased: {
                    if (root._dragging) {
                        root._dragging = false
                        root.moved(root.x)
                    }
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }

    // =========================================================
    // КОНТЕКСТНОЕ МЕНЮ ВИДЕО
    // =========================================================
    Menu {
        id: videoMenu

        background: Rectangle {
            color: Theme.panelBackground
            radius: Theme.borderRadius
            border.color: Theme.rubyPrimary
            border.width: 1
        }

        MenuItem {
            text: "✂  Разрезать"
            onTriggered: root.splitRequested(root.clipId)
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            background: Rectangle {
                color: parent.hovered ? Theme.buttonHover : "transparent"
                radius: Theme.borderRadius
            }
        }
        MenuItem {
            text: "✨  Эффекты..."
            onTriggered: root.effectsRequested(root.clipId)
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            background: Rectangle {
                color: parent.hovered ? Theme.buttonHover : "transparent"
                radius: Theme.borderRadius
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "🗑  Удалить"
            onTriggered: root.deleteRequested(root.clipId)
            contentItem: Text {
                text: parent.text
                color: "#EF5350"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            background: Rectangle {
                color: parent.hovered ? Qt.rgba(0.94, 0.33, 0.31,
                                                0.15) : "transparent"
                radius: Theme.borderRadius
            }
        }
    }

    // =========================================================
    // КОНТЕКСТНОЕ МЕНЮ АУДИО
    // =========================================================
    Menu {
        id: audioMenu

        background: Rectangle {
            color: Theme.panelBackground
            radius: Theme.borderRadius
            border.color: "#43A047"
            border.width: 1
        }

        MenuItem {
            text: root.isMuted ? "🔊  Включить звук" : "🔇  Отключить звук"
            onTriggered: {
                root.isMuted = !root.isMuted
                root.muteToggled(root.clipId, root.isMuted)
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            background: Rectangle {
                color: parent.hovered ? Theme.buttonHover : "transparent"
                radius: Theme.borderRadius
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "🗑  Удалить клип"
            onTriggered: root.deleteRequested(root.clipId)
            contentItem: Text {
                text: parent.text
                color: "#EF5350"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
            background: Rectangle {
                color: parent.hovered ? Qt.rgba(0.94, 0.33, 0.31,
                                                0.15) : "transparent"
                radius: Theme.borderRadius
            }
        }
    }
}
