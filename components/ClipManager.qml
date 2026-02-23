import QtQuick
import "../theme.js" as Theme

Item {
    id: clipManager
    visible: false

    // ===== ДОБАВИТЬ КЛИП (ДЕЛЕГИРУЕМ В C++!) =====
    function addClip(filepath, trackNumber, startTime) {
        console.log("   ClipManager: Вызываю cppTimeline.addClip")
        console.log("   filepath:", filepath)
        console.log("   trackNumber:", trackNumber)
        console.log("   startTime:", startTime)

        var success = cppTimeline.addClip(filepath, trackNumber, startTime)

        if (success) {
            console.log("✅ C++ успешно добавил клип!")
        } else {
            console.log("❌ C++ не смог добавить клип")
        }

        // C++ сам emit clipsChanged() → QML обновится!
    }

    // ===== СЛУШАЕМ СИГНАЛЫ ОТ C++ =====
    Connections {
        target: cppTimeline

        function onClipsChanged() {
            console.log(" Получен сигнал: clipsChanged от C++")
            // Timeline.qml автоматически обновится!
        }

        function onClipAdded(index) {
            console.log(" Клип добавлен, индекс:", index)
        }

        function onClipRemoved(index) {
            console.log(" Клип удалён, индекс:", index)
        }

        function onTotalDurationChanged() {
            console.log(" Длительность timeline изменилась")
        }
    }
}
