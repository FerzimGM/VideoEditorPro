import QtQuick
import "../theme.js" as Theme
//Только логирование

Item {
    id: clipManager
    visible: false

    //  ДОБАВИТЬ КЛИП (ДЕЛЕГИРУЕМ В C++!)
    function addClip(filepath, trackNumber, startTime) {
        if (DEBUG_MODE)
        console.log("   ClipManager: Вызываю cppTimeline.addClip")
        if (DEBUG_MODE)
        console.log("   filepath:", filepath)
        if (DEBUG_MODE)
        console.log("   trackNumber:", trackNumber)
        if (DEBUG_MODE)
        console.log("   startTime:", startTime)

        var success = cppTimeline.addClip(filepath, trackNumber, startTime)

        if (success) {
            if (DEBUG_MODE)
            console.log("✅ C++ успешно добавил клип!")
        } else {
            if (DEBUG_MODE)
            console.log("❌ C++ не смог добавить клип")
        }

        // C++ сам emit clipsChanged() → QML обновится!
    }

    // СЛУШАЕМ СИГНАЛЫ ОТ C++
    Connections {
        target: cppTimeline

        function onClipsChanged() {
            if (DEBUG_MODE)
            console.log(" Получен сигнал: clipsChanged от C++")
            // Timeline.qml автоматически обновится!
        }

        function onClipAdded(index) {
            if (DEBUG_MODE)
            console.log(" Клип добавлен, индекс:", index)
        }

        function onClipRemoved(index) {
            if (DEBUG_MODE)
            console.log(" Клип удалён, индекс:", index)
        }

        function onTotalDurationChanged() {
            if (DEBUG_MODE)
            console.log(" Длительность timeline изменилась")
        }
    }
}
