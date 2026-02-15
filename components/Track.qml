import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

Rectangle {
    id: root
    color: Theme.trackBackground

    property int trackNumber: 1
    property real duration: 100
    property real pixelsPerSecond: 10
    property real currentTime: 0
    property var clips: []
    property var clipManager: null  // Ссылка на ClipManager

    signal clipDropped(string filepath, real time)
    signal clipMoved(int clipId, real newTime)
    
    // Обновляем clips когда clipManager изменяется
    onClipManagerChanged: {
        if (clipManager) {
            updateClips()
        }
    }
    
    // Подключаемся к изменениям в clipManager
    Connections {
        target: clipManager
        function onRevisionChanged() {
            root.updateClips()
        }
    }
    
    function updateClips() {
        if (clipManager) {
            var newClips = clipManager.getClipsForTrack(trackNumber)
            // Обновляем только если изменилось количество
            if (!clips || clips.length !== newClips.length) {
                clips = newClips
                console.log("Track", trackNumber, "обновлён, клипов:", clips.length)
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.dividerColor
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // Видео дорожка
        Rectangle {
            width: parent.width
            height: Theme.trackHeight
            color: Qt.darker(Theme.trackBackground, 1.05)

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.dividerColor
            }

            // Drop area для видео
            DropArea {
                anchors.fill: parent
                keys: ["text/uri-list"]  // Стандартный MIME type для файлов
                
                onDropped: (drop) => {
                    console.log("=== DROP EVENT ===")
                    console.log("hasUrls:", drop.hasUrls)
                    if (drop.hasUrls) {
                        var url = drop.urls[0].toString()
                        console.log("URL raw:", url)
                        url = url.replace(/^file:\/\/\//, "")
                        console.log("URL cleaned:", url)
                        var time = (drop.x / root.pixelsPerSecond)
                        console.log("Calling root.clipDropped...")
                        root.clipDropped(url, time)
                        console.log("Видео дроп:", url, "время:", time)
                    } else {
                        console.log("NO URLs in drop!")
                    }
                }
                
                Rectangle {
                    anchors.fill: parent
                    color: parent.containsDrag ? Qt.rgba(0.78, 0.15, 0.31, 0.15) : "transparent"
                }
            }

            // Отображение видео клипов
            Repeater {
                model: root.clips

                VideoClip {
                    x: modelData.startTime * root.pixelsPerSecond
                    y: 5
                    width: modelData.duration * root.pixelsPerSecond
                    height: Theme.trackHeight - 10
                    
                    clipName: modelData.filename
                    clipId: modelData.id  // Передаём ID!
                    selected: modelData.selected
                    
                    Component.onCompleted: {
                        console.log("VideoClip создан:", modelData.filename, "x:", x, "width:", width)
                    }
                    
                    onMoved: (newX) => {
                        var newTime = newX / root.pixelsPerSecond
                        root.clipMoved(modelData.id, newTime)
                    }
                    
                    onDeleteRequested: (clipId) => {
                        console.log("Track: удаление клипа", clipId)
                        if (root.clipManager) {
                            root.clipManager.removeClip(clipId)
                        }
                    }
                }
            }

            // Подсказка если пусто
            Text {
                anchors.centerIn: parent
                text: "Перетащите видео сюда"
                color: Theme.textDisabled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                opacity: root.clips.length === 0 ? 0.3 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // Аудио дорожка
        Rectangle {
            width: parent.width
            height: Theme.trackHeight
            color: Qt.darker(Theme.trackBackground, 1.1)

            // Drop area для аудио
            DropArea {
                anchors.fill: parent
                keys: ["text/uri-list"]
                
                onDropped: (drop) => {
                    if (drop.hasUrls) {
                        var url = drop.urls[0].toString()
                        url = url.replace(/^file:\/\/\//, "")
                        var time = (drop.x / root.pixelsPerSecond)
                        console.log("Аудио дроп:", url, "время:", time)
                        // Пока только логируем, аудио клипы добавим позже
                    }
                }
                
                Rectangle {
                    anchors.fill: parent
                    color: parent.containsDrag ? Qt.rgba(0.18, 0.8, 0.44, 0.1) : "transparent"
                }
            }

            // Подсказка
            Text {
                anchors.centerIn: parent
                text: "Аудио дорожка"
                color: Theme.textDisabled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                opacity: 0.2
            }
        }
    }
}
