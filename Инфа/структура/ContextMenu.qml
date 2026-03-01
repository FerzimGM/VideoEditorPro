// components/ContextMenu.qml
import QtQuick
import QtQuick.Controls
import "../theme.js" as Theme

Item {
    id: contextMenuRoot

    property var cppTimeline: null
    property int _ctxClipId: -1
    property int _ctxTrack: 1
    property bool _ctxIsMuted: false
    property string _ctxClipName: ""

    signal effectsRequested(int clipId)

    Menu {
        id: videoMenu
        parent: Overlay.overlay // ← Показывать поверх ВСЕГО

        background: Rectangle {
            color: Theme.panelBackground
            radius: Theme.borderRadius
            border.color: Theme.rubyPrimary
            border.width: 1
        }

        MenuItem {
            enabled: false
            contentItem: Text {
                text: "📹  " + contextMenuRoot._ctxClipName
                color: Theme.textSecondary
                font.bold: true
            }
            background: Rectangle {
                color: "transparent"
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "✂  Разрезать по playhead"
            onTriggered: if (cppTimeline)
                             cppTimeline.splitClipAt(cppTimeline.currentTime,
                                                     contextMenuRoot._ctxTrack)
        }

        MenuItem {
            text: "✨  Эффекты клипа..."
            onTriggered: contextMenuRoot.effectsRequested(
                             contextMenuRoot._ctxClipId)
        }

        MenuSeparator {}

        MenuItem {
            text: "🗑  Удалить клип"
            onTriggered: if (cppTimeline)
                             cppTimeline.removeClip(contextMenuRoot._ctxClipId)
            contentItem: Text {
                text: parent.text
                color: "#EF5350"
            }
        }
    }

    Menu {
        id: audioMenu
        parent: Overlay.overlay

        background: Rectangle {
            color: Theme.panelBackground
            radius: Theme.borderRadius
            border.color: "#43A047"
            border.width: 1
        }

        MenuItem {
            enabled: false
            contentItem: Text {
                text: "🎵  " + contextMenuRoot._ctxClipName
                color: Theme.textSecondary
                font.bold: true
            }
            background: Rectangle {
                color: "transparent"
            }
        }

        MenuSeparator {}

        MenuItem {
            text: contextMenuRoot._ctxIsMuted ? "🔊  Включить" : "🔇  Выключить"
            onTriggered: {
                contextMenuRoot._ctxIsMuted = !contextMenuRoot._ctxIsMuted
                if (cppTimeline)
                    cppTimeline.setClipMuted(contextMenuRoot._ctxClipId,
                                             contextMenuRoot._ctxIsMuted)
            }
        }

        // ... остальные пункты как в видео меню ...
    }

    function showVideoMenu(clipId, track, clipName, x, y) {
        _ctxClipId = clipId
        _ctxTrack = track
        _ctxClipName = clipName
        // popup() без аргументов — Qt сам использует текущую позицию курсора.
        // Это единственный надёжный способ для контекстного меню по ПКМ в Qt 6:
        // mapFromGlobal на Overlay.overlay нестабилен на Windows,
        // popup(null, x, y) игнорирует координаты при null-parent.
        videoMenu.popup()
    }

    function showAudioMenu(clipId, track, clipName, isMuted, x, y) {
        _ctxClipId = clipId
        _ctxTrack = track
        _ctxClipName = clipName
        _ctxIsMuted = isMuted
        audioMenu.popup()
    }
}
