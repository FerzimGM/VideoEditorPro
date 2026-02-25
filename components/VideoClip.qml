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
    property bool selected: false

    // Максимальная ширина клипа = оригинальная длительность * pixelsPerSecond
    // Устанавливается из Track.qml при создании делегата и при изменении зума.
    // Правый handle НИКОГДА не позволит растянуть клип шире этого значения.
    property real clipMaxWidth: 0

    readonly property real videoH: 50
    readonly property real audioH: 30
    height: videoH + audioH + 2

    // ===== СИГНАЛЫ =====
    signal moved(real newX)
    signal clicked
    signal deleteRequested(int clipId)
    signal splitRequested(int clipId)
    signal effectsRequested(int clipId)
    signal muteToggled(int clipId, bool muted)

    // ===== DRAG STATE =====
    property real _startX: 0
    property real _dragOfsX: 0
    property bool _dragging: false

    // ===== DRAG API — для переноса клипа между дорожками =====
    // Когда _dragging=true, Qt Drag&Drop система следит за позицией hotspot
    // и сигнализирует DropArea нужной дорожки, даже если визуально клип
    // ещё отображается на исходной дорожке.
    Drag.active: _dragging
    Drag.keys: ["clip/move"]
    Drag.mimeData: {
        "clip/id": String(root.clipId)
    }
    Drag.supportedActions: Qt.MoveAction
    Drag.hotSpot.x: 0
    Drag.hotSpot.y: 0

    // ===== SHARED DRAG HELPERS =====
    function beginDrag(mouseX, mouseY) {
        root._startX = root.x
        root._dragOfsX = mouseX
        root._dragging = true
        root.clicked()
    }

    // Вызывается из onPositionChanged MouseArea.
    // mapSourceItem — сам MouseArea, нужен для mapToGlobal.
    function updateDrag(mouseX, mouseY, mapSourceItem) {
        root.x = root._startX + (mouseX - root._dragOfsX)
        // Обновляем hotspot в координатах root-Item по глобальной позиции мыши.
        // Это позволяет DropArea другой дорожки обнаружить перетаскивание.
        var gPos = mapSourceItem.mapToGlobal(mouseX, mouseY)
        var lPos = root.mapFromGlobal(gPos.x, gPos.y)
        root.Drag.hotSpot.x = lPos.x
        root.Drag.hotSpot.y = lPos.y
    }

    // Вызывается из onReleased MouseArea.
    function finishDrag() {
        root._dragging = false
        var result = root.Drag.drop()
        if (result === Qt.IgnoreAction) {
            // Не попали в DropArea другой дорожки → перенос внутри своей
            root.moved(root.x)
        }
        // Если result === Qt.MoveAction — Track.qml DropArea сам вызвал cppTimeline.moveClip
    }

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

            // Метка V
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

            // ── Левый resize handle ──
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
                    property real _ox: 0
                    onPressed: {
                        _sx = mouse.x
                        _sw = root.width
                        _ox = root.x
                    }
                    onPositionChanged: {
                        if (pressed) {
                            var d = mouse.x - _sx
                            var nw = _sw - d
                            // Ограничение: не шире оригинальной длины, не уже 30px
                            if (root.clipMaxWidth > 0)
                                nw = Math.min(nw, root.clipMaxWidth)
                            if (nw >= 30) {
                                root.x = _ox + d
                                root.width = nw
                            }
                        }
                    }
                    onReleased: root.moved(root.x)
                }
            }

            // ── Правый resize handle — ОГРАНИЧЕН clipMaxWidth ──
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
                            // ГЛАВНОЕ ОГРАНИЧЕНИЕ: нельзя растянуть длиннее оригинала
                            if (root.clipMaxWidth > 0)
                                nw = Math.min(nw, root.clipMaxWidth)
                            nw = Math.max(30, nw)
                            root.width = nw
                        }
                    }
                    onReleased: root.moved(root.x)
                }
            }

            // ── Drag + контекстное меню видео ──
            MouseArea {
                id: videoDragMA
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
                        // Используем глобальные координаты — это единственный надёжный
                        // способ показать popup в Qt Quick Controls 2 внутри Flickable
                        var gPos = mapToGlobal(mouse.x, mouse.y)
                        videoMenu.popup(null, gPos.x, gPos.y)
                    } else {
                        root.beginDrag(mouse.x, mouse.y)
                        grabMouse() // Получаем события даже вне своего Rectangle
                    }
                }
                onPositionChanged: function (mouse) {
                    if (root._dragging)
                        root.updateDrag(mouse.x, mouse.y, videoDragMA)
                }
                onReleased: function (mouse) {
                    if (root._dragging) {
                        ungrabMouse()
                        root.finishDrag()
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

            // Псевдо-осциллограмма
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
                    model: Math.max(0, Math.floor((audioStrip.width - 30) / 4))
                    delegate: Rectangle {
                        width: 2
                        height: Math.abs(
                                    Math.sin(
                                        index * 0.63 + 0.5)) * (audioStrip.height * 0.65) + 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(1, 1, 1, root.isMuted ? 0.15 : 0.35)
                        radius: 1
                    }
                }
            }

            // ── Drag + контекстное меню аудио ──
            MouseArea {
                id: audioDragMA
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root._dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onPressed: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        root.clicked()
                        var gPos = mapToGlobal(mouse.x, mouse.y)
                        audioMenu.popup(null, gPos.x, gPos.y)
                    } else {
                        root.beginDrag(mouse.x, mouse.y)
                        grabMouse()
                    }
                }
                onPositionChanged: function (mouse) {
                    if (root._dragging)
                        root.updateDrag(mouse.x, mouse.y, audioDragMA)
                }
                onReleased: function (mouse) {
                    if (root._dragging) {
                        ungrabMouse()
                        root.finishDrag()
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
