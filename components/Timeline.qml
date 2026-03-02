import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.timelineBackground

    property real currentTime: 0
    property real duration: cppTimeline ? Math.max(
                                              120, cppTimeline.totalDuration
                                              + 20) : 120 // Минимум 100 секунд
    property int zoomLevel: 100
    property real pixelsPerSecond: 10
    property bool snapEnabled: true // Snap включён

    // *** Клипы обеих дорожек для cross-track магнит-snap ***
    // Каждой дорожке передаём клипы ДРУГОЙ дорожки, чтобы магнит
    // притягивался к концам клипов на обеих дорожках одновременно.
    property var track1Clips: []
    property var track2Clips: []

    // ✅ Отслеживать изменения snapEnabled
    onSnapEnabledChanged: {
        console.log("🧲 Snap:", snapEnabled ? "ВКЛ" : "ВЫКЛ")
    }

    Connections {
        target: cppTimeline
        function onClipsChanged() {
            root.track1Clips = cppTimeline ? cppTimeline.getClipsForTrack(
                                                 1) : []
            root.track2Clips = cppTimeline ? cppTimeline.getClipsForTrack(
                                                 2) : []
        }
    }

    Component.onCompleted: {
        root.track1Clips = cppTimeline ? cppTimeline.getClipsForTrack(1) : []
        root.track2Clips = cppTimeline ? cppTimeline.getClipsForTrack(2) : []
    }

    // *** Выделение клипов через property-цепочку (main → Timeline → Track → VideoClip) ***
    // Прямой доступ по ID между компонентами в QML не работает!
    property int selectedClipId: -1
    property var clipStates: null
    property int _ctxClipId: -1
    property int _ctxTrack: 1
    property bool _ctxIsMuted: false
    property string _ctxClipName: ""
    signal clipSelected(int clipId)

    signal timeChanged(real time)
    signal zoomChanged(int zoom)
    signal effectsRequested(int clipId)
    signal showVideoContextMenu(int clipId, int track, string clipName, real x, real y)
    signal showAudioContextMenu(int clipId, int track, string clipName, bool isMuted, real x, real y)

    onZoomLevelChanged: {
        pixelsPerSecond = zoomLevel / 10
    }

    // Обновляем duration при добавлении клипов
    Connections {
        target: cppTimeline
        function onTotalDurationChanged() {
            console.log("📏 Общая длительность:", cppTimeline.totalDuration)
        }
        function onClipsChanged() {
            console.log("📋 Clips changed")
            // ВАЖНО: НЕ делаем root.selectedClipId = -1 напрямую!
            // Прямое JS-присваивание в QML разрушает binding навсегда.
            // Binding: selectedClipId: selectionManager.selectedClipId (в main.qml)
            // После прямого присваивания selectionManager больше не обновляет Timeline.
            // Решение: только сигнал → main.qml сбросит selectionManager → binding сработает.
            root.clipSelected(-1)
        }
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 2
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.rubyGradientStart
            }
            GradientStop {
                position: 1.0
                color: Theme.rubyGradientEnd
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Левая панель с номерами дорожек
        Rectangle {
            Layout.preferredWidth: 120
            Layout.fillHeight: true
            color: Theme.panelBackground

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.dividerColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: Theme.backgroundDark

                    Text {
                        anchors.centerIn: parent
                        text: "ДОРОЖКИ"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.dividerColor
                    }
                }

                Repeater {
                    model: 2
                    TrackLabel {
                        trackNumber: index + 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.trackHeight * 2
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Область таймлайна
        Flickable {
            id: timelineFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: Math.max(width,
                                   root.duration * root.pixelsPerSecond + 100)
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AlwaysOn
                contentItem: Rectangle {
                    implicitHeight: 8
                    radius: 4
                    color: Theme.rubyPrimary
                    opacity: parent.pressed ? 0.8 : (parent.hovered ? 0.6 : 0.4)
                }
            }

            // Масштабирование колесиком мыши (Ctrl+scroll)
            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                acceptedButtons: Qt.NoButton
                onWheel: wheel => {
                             if (wheel.modifiers & Qt.ControlModifier) {
                                 var delta = wheel.angleDelta.y > 0 ? 10 : -10
                                 var newZoom = Math.max(
                                     20, Math.min(500, root.zoomLevel + delta))
                                 root.zoomChanged(newZoom)
                                 wheel.accepted = true
                             } else {
                                 wheel.accepted = false
                             }
                         }
            }

            ColumnLayout {
                width: Math.max(timelineFlickable.width,
                                timelineFlickable.contentWidth)
                height: timelineFlickable.height
                spacing: 0

                // Временная шкала
                TimeRuler {
                    id: timeRuler
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    duration: root.duration
                    pixelsPerSecond: root.pixelsPerSecond
                    currentTime: root.currentTime
                }

                // ***  используем root.pixelsPerSecond и root.snapEnabled (не timeline.*) ***
                Repeater {
                    model: 2

                    Track {
                        trackNumber: index + 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.trackHeight * 2
                        //  root.pixelsPerSecond (не timeline.pixelsPerSeconde — опечатка!)
                        pixelsPerSecond: root.pixelsPerSecond
                        //  root.snapEnabled (не timeline.snapEnabled)
                        snapEnabled: root.snapEnabled
                        // *** Клипы другой дорожки для cross-track магнит-snap ***
                        otherTrackClips: index === 0 ? root.track2Clips : root.track1Clips
                        // *** Передаём выделение вниз по цепочке ***
                        selectedClipId: root.selectedClipId
                        clipStates: root.clipStates

                        // Когда пользователь кликнул по клипу внутри Track
                        onClipSelected: id => {
                                            root.clipSelected(id)
                                        }

                        // Когда видео дропнули на дорожку
                        onClipDropped: (filepath, time) => {
                                           console.log(
                                               "=== Timeline.onClipDropped ===")
                                           console.log("filepath:", filepath)
                                           console.log("time:", time)
                                           console.log("track:", index + 1)
                                           //  используем глобальный cppTimeline (не timeline.cppTimeline)
                                           if (cppTimeline) {
                                               console.log(
                                                   "Calling cppTimeline.addClip...")
                                               cppTimeline.addClip(filepath,
                                                                   index + 1,
                                                                   time)
                                           } else {
                                               console.log(
                                                   "ERROR: cppTimeline is NULL!")
                                           }
                                       }

                        // Когда клип подвинули
                        onClipMoved: (clipId, newTime) => {
                                         //  используем глобальный cppTimeline (не root.cppTimeline)
                                         if (cppTimeline) {
                                             cppTimeline.moveClip(clipId,
                                                                  index + 1,
                                                                  newTime)
                                         }
                                     }
                        onEffectsRequested: id => root.effectsRequested(id)

                        onContextMenuRequested: (id, isVideo, gx, gy) => {
                                                    // Сохраняем контекст
                                                    var clips = cppTimeline ? cppTimeline.getClipsForTrack(index + 1) : []
                                                    root._ctxClipId = id
                                                    root._ctxTrack = index + 1

                                                    for (var i = 0; i < clips.length; i++) {
                                                        if (clips[i].id === id) {
                                                            root._ctxClipName = clips[i].filename
                                                            || ""
                                                            root._ctxIsMuted = clips[i].isMuted
                                                            || false
                                                            break
                                                        }
                                                    }

                                                    // Эмитим сигналы (не popup!)
                                                    if (isVideo)
                                                    root.showVideoContextMenu(
                                                        id, index + 1,
                                                        root._ctxClipName, gx,
                                                        gy)
                                                    else
                                                    root.showAudioContextMenu(
                                                        id, index + 1,
                                                        root._ctxClipName,
                                                        root._ctxIsMuted, gx,
                                                        gy)
                                                }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            // *** Линия воспроизведения (Playhead) ***
            Rectangle {
                id: playhead
                //  x привязан к currentTime * pixelsPerSecond
                x: root.currentTime * root.pixelsPerSecond
                y: 0
                width: 2
                height: timelineFlickable.contentHeight
                color: Theme.rubyPrimary
                z: 1000

                // Треугольник сверху
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -10
                    width: 14
                    height: 22
                    color: Theme.rubyPrimary
                    radius: 2

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.fillStyle = Theme.rubyPrimary
                            ctx.beginPath()
                            ctx.moveTo(7, 22)
                            ctx.lineTo(0, 16)
                            ctx.lineTo(14, 16)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }
                }

                // Временной код на playhead
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 5
                    width: playheadTime.width + 10
                    height: playheadTime.height + 6
                    color: Qt.rgba(0, 0, 0, 0.9)
                    radius: 3
                    border.color: Theme.rubyPrimary
                    border.width: 1

                    Text {
                        id: playheadTime
                        anchors.centerIn: parent
                        text: formatTime(root.currentTime)
                        color: Theme.rubyLight
                        font.family: "Consolas, monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Behavior on x {
                    enabled: !playheadMouseArea.drag.active
                    NumberAnimation {
                        duration: 50
                    }
                }
            }

            // *** ИСПРАВЛЕНО: MouseArea только над TimeRuler (высота 40px) ***
            // Раньше anchors.fill покрывал весь flickable (z:999) и перехватывал
            // все клики на клипы — они никогда не доходили до VideoClip.
            MouseArea {
                id: playheadMouseArea
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 40 // Только зона линейки
                z: 999
                acceptedButtons: Qt.LeftButton

                onClicked: mouse => {
                               // *** FIX: mouse.x уже внутри flickable — НЕ добавляем contentX! ***
                               // mouse.x — это позиция относительно начала контента flickable
                               var time = mouse.x / root.pixelsPerSecond
                               root.timeChanged(
                                   Math.max(0, Math.min(root.duration, time)))
                           }
            }

            // *** УДАЛЕНО: MouseArea ниже линейки ***
            // ПРИЧИНА УДАЛЕНИЯ: в QML при одинаковом z побеждает тот, кто объявлен ПОЗЖЕ.
            // Эта MouseArea была объявлена ПОСЛЕ ColumnLayout с клипами → перехватывала
            // ВСЕ клики на клипы, VideoClip никогда не получал события мыши.
            // Снятие выделения — через Escape (Shortcut в main.qml).
        }
    }

    function formatTime(seconds) {
        var hours = Math.floor(seconds / 3600)
        var mins = Math.floor((seconds % 3600) / 60)
        var secs = Math.floor(seconds % 60)
        var frames = Math.floor((seconds % 1) * 30)
        if (hours > 0) {
            return pad(hours) + ":" + pad(mins) + ":" + pad(secs)
        }
        return pad(mins) + ":" + pad(secs) + ":" + pad(frames)
    }

    function pad(num) {
        return num < 10 ? "0" + num : String(num)
    }

    component TrackLabel: Rectangle {
        property int trackNumber: 1
        color: Theme.panelBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.dividerColor
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacingSmall

            Text {
                text: "Дорожка " + trackNumber
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.alignment: Qt.AlignHCenter

                Rectangle {
                    width: 18
                    height: 18
                    radius: 2
                    color: Theme.clipColor
                    Text {
                        anchors.centerIn: parent
                        text: "V"
                        color: "#FFF"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
                Rectangle {
                    width: 18
                    height: 18
                    radius: 2
                    color: Theme.waveformColor
                    Text {
                        anchors.centerIn: parent
                        text: "A"
                        color: "#FFF"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }
    }
}
