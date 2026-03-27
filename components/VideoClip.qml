import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Item {
    id: root

    // СВОЙСТВА
    property string clipName: "Clip"
    property int clipId: -1
    property bool isMuted: false
    property bool selected: false
    property real clipMaxWidth: 0
    property real pixelsPerSecond: 10
    property int trackNumber: 1

    property bool videoHidden: false
    property bool audioHidden: false

    readonly property real videoH: 50
    readonly property real audioH: 30
    height: videoH + audioH + 2

    // СИГНАЛЫ
    signal moved(real newX)
    signal rightTrimmed(int clipId, real newPixelWidth) // новая ширина в пикселях
    signal clicked
    signal deleteRequested(int clipId)
    signal splitRequested(int clipId)
    signal effectsRequested(int clipId)
    signal muteToggled(int clipId, bool muted)
    // Сигнал для глобального меню: передаём глобальные экранные координаты
    // и флаг isVideo чтобы Timeline.qml знал какое меню показать
    signal contextMenuRequested(int clipId, bool isVideo, real globalX, real globalY)

    // DRAG STATE
    property real _startX: 0
    property bool _dragging: false

    //  DRAG API
    Drag.keys: ["clip/move"]
    Drag.mimeData: {
        "clip/id": String(root.clipId)
    }
    Drag.supportedActions: Qt.MoveAction
    Drag.hotSpot.x: 0
    Drag.hotSpot.y: 0

    // COLUMN: ВИДЕО + АУДИО
    Column {
        anchors.fill: parent
        spacing: 2

        // ВИДЕО ПОЛОСА (синяя)
        Rectangle {
            id: videoStrip
            width: parent.width
            height: root.videoH
            opacity: root.videoHidden ? 0.4 : 1.0
            radius: Theme.borderRadius
            color: root.videoHidden ? "#555" : (root.selected ? "#1E88E5" : "#1565C0")
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

            Text {
                anchors {
                    left: vLabel.right
                    right: rightHandle.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: 4
                    rightMargin: 4
                }
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
                            if (root.clipMaxWidth > 0)
                                nw = Math.min(nw, root.clipMaxWidth)
                            if (nw >= 30) {
                                root.x = _ox + d
                                root.width = nw
                            }
                        }
                    }
                    onReleased: {
                        // Один атомарный вызов C++: обновляет startTime + trimStart + duration.
                        // НЕ вызываем moved() отдельно — setClipLeftTrim сам делает всё.
                        var pps = root.pixelsPerSecond || 10
                        var newStartTime = root.x / pps
                        var deltaX = root.x - _ox // px
                        var deltaSec = deltaX / pps // секунды

                        if (cppTimeline) {
                            // Берём текущий trimStart из C++
                            var info = cppTimeline.getClipInfoAt(
                                        (_ox + _sw * 0.5) / pps,
                                        root.trackNumber || 1)
                            var currentTrimStart = (info && info.trimStart
                                                    !== undefined) ? info.trimStart : 0.0
                            var newTrimStart = Math.max(
                                        0.0, currentTrimStart + deltaSec)

                            if (DEBUG_MODE) {
                                console.log("✂ LeftTrim id=", root.clipId,
                                            "Δsec=", deltaSec.toFixed(3),
                                            "trimStart:",
                                            currentTrimStart.toFixed(3), "→",
                                            newTrimStart.toFixed(3))
                            }

                            cppTimeline.setClipLeftTrim(root.clipId,
                                                        newStartTime,
                                                        newTrimStart)
                        } else {
                            // Fallback: просто двигаем позицию
                            root.moved(root.x)
                        }
                    }
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
                            if (root.clipMaxWidth > 0)
                                nw = Math.min(nw, root.clipMaxWidth)
                            root.width = Math.max(30, nw)
                        }
                    }
                    onReleased: {
                        root.rightTrimmed(root.clipId, root.width)
                    }
                }
            }



            //  ЛЕВЫЙ КЛИК - выделение клипа
            TapHandler {
                id: videoTapLeft
                acceptedButtons: Qt.LeftButton
                onTapped: function (eventPoint) {
                    root.clicked()
                }
            }

            // Правый клик обрабатывается rootRightClick на уровне root Item

            // ПЕРЕТАСКИВАНИЕ — только левая кнопка
            DragHandler {
                id: videoDragHandler
                target: null
                acceptedButtons: Qt.LeftButton
                grabPermissions: PointerHandler.CanTakeOverFromAnything

                onActiveChanged: {
                    if (active) {
                        root._startX = root.x
                        root._dragging = true
                        // выделяем при начале drag тоже
                        root.clicked()

                        var lp = videoStrip.mapToItem(root,
                                                      centroid.position.x,
                                                      centroid.position.y)
                        root.Drag.hotSpot.x = lp.x
                        root.Drag.hotSpot.y = lp.y

                        // Запускаем drag-сессию ВРУЧНУЮ
                        root.Drag.active = true
                    } else {
                        root._dragging = false

                        // КРИТИЧНО: drop() ДО Drag.active = false!
                        // Если поменять местами — Qt закроет сессию до drop(),
                        // DropArea на другой дорожке не получит событие.
                        var result = root.Drag.drop()
                        root.Drag.active = false

                        if (result === Qt.IgnoreAction) {
                            // Не попали в DropArea другой дорожки - двигаем внутри своей
                            root.moved(root.x)
                        }
                        // Qt.MoveAction - Track.qml clipMoveDropArea вызвал cppTimeline.moveClip
                    }
                }

                onTranslationChanged: {
                    if (active) {
                        root.x = Math.max(0, root._startX + translation.x)

                        // mapToGlobal + mapFromGlobal — надёжный способ для Qt 6,
                        // особенно внутри Flickable с contentX смещением
                        var gp = videoStrip.mapToGlobal(centroid.position.x,
                                                        centroid.position.y)
                        var lp = root.mapFromGlobal(gp.x, gp.y)
                        root.Drag.hotSpot.x = lp.x
                        root.Drag.hotSpot.y = lp.y
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.videoHidden
                text: "🚫 видео скрыто"
                color: "#ccc"
                font.pixelSize: 10
                font.bold: true
            }
            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }

        // АУДИО ПОЛОСА (зелёная)
        Rectangle {
            id: audioStrip
            width: parent.width
            height: root.audioH
            opacity: (root.audioHidden || root.isMuted) ? 0.5 : 1.0
            radius: Theme.borderRadius
            color: root.audioHidden ? "#555" : (root.isMuted ? "#555" : (root.selected ? "#43A047" : "#2E7D32"))
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
                    leftMargin: 4
                    rightMargin: 4
                }
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

            // ЛЕВЫЙ КЛИК - выделение
            TapHandler {
                id: audioTapLeft
                acceptedButtons: Qt.LeftButton
                onTapped: function (eventPoint) {
                    root.clicked()
                }
            }

            // Правый клик обрабатывается rootRightClick на уровне root Item

            // ПЕРЕТАСКИВАНИЕ аудио полосы
            DragHandler {
                id: audioDragHandler
                target: null
                acceptedButtons: Qt.LeftButton
                grabPermissions: PointerHandler.CanTakeOverFromAnything

                onActiveChanged: {
                    if (active) {
                        root._startX = root.x
                        root._dragging = true
                        root.clicked()

                        var lp = audioStrip.mapToItem(root,
                                                      centroid.position.x,
                                                      centroid.position.y)
                        root.Drag.hotSpot.x = lp.x
                        root.Drag.hotSpot.y = lp.y

                        root.Drag.active = true
                    } else {
                        root._dragging = false

                        // КРИТИЧНО: drop() ДО Drag.active = false!
                        var result = root.Drag.drop()
                        root.Drag.active = false

                        if (result === Qt.IgnoreAction) {
                            root.moved(root.x)
                        }
                    }
                }

                onTranslationChanged: {
                    if (active) {
                        root.x = Math.max(0, root._startX + translation.x)

                        var gp = audioStrip.mapToGlobal(centroid.position.x,
                                                        centroid.position.y)
                        var lp = root.mapFromGlobal(gp.x, gp.y)
                        root.Drag.hotSpot.x = lp.x
                        root.Drag.hotSpot.y = lp.y
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.audioHidden || root.isMuted
                text: root.audioHidden ? "🔇 аудио скрыто" : "🔇 заглушено"
                color: "#ccc"
                font.pixelSize: 9
                font.bold: true
            }
            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }

    // ПРАВЫЙ КЛИК — эмитит сигнал наверх в Timeline → main.qml
    // Меню объявлены ПРЯМО В main.qml (ApplicationWindow) — только там
    // Overlay.overlay работает корректно в Qt 6.
    MouseArea {
        id: rootRightClick
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        z: 100
        // ВАЖНО: onPressed, не onClicked!
        // onClicked = mouseRelease. Если вызвать popup() на release — Qt отправляет
        // следующий MouseButtonRelease в меню как "клик вне меню" - мгновенное закрытие.
        // onPressed = кнопка ЕЩЁ ЗАЖАТА когда popup() открывается - release происходит
        // уже внутри открытого меню - меню остаётся живым.
        onPressed: function (mouse) {
            if (mouse.button !== Qt.RightButton)
                return
            mouse.accepted = true
            var g = root.mapToGlobal(mouse.x, mouse.y)
            var isVideo = (mouse.y < root.videoH + 1)
            if (DEBUG_MODE) {
                console.log("🖱️ ПКМ", isVideo ? "VIDEO" : "AUDIO", "clipId=",
                            root.clipId)
            }
            root.contextMenuRequested(root.clipId, isVideo, g.x, g.y)
        }
    }


    // РАМКА ВЫДЕЛЕНИЯ — root уровень (поверх video И audio полос)
    // КРИТИЧНО: эта рамка должна быть ЗДЕСЬ — прямым потомком root Item,
    // а НЕ внутри videoStrip. Причина: anchors.fill: parent у потомка Rectangle
    // заполняет область ВНУТРИ border родителя. Если border рамки и border
    // videoStrip одного цвета (rubyPrimary) — они сливаются и рамка невидима.
    // На уровне root: Rectangle покрывает ВСЕ дочерние элементы, z:50 — поверх всего.
    Rectangle {
        id: selectionBorder
        anchors.fill: parent
        radius: Theme.borderRadius
        color: "transparent"
        border.color: Theme.rubyPrimary
        border.width: root.selected ? 3 : 0
        z: 50
        enabled: false // не блокируем клики под собой!
        // Лёгкая рубиновая подсветка внутри рамки
        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: parent.radius - 3
            color: Qt.rgba(0.87, 0.13, 0.23, 0.08)
            visible: root.selected
        }
    }
}
