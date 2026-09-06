import QtQuick
import "../theme.js" as Theme

/**
 * ClipManager
 * -----------
 * Non-visual bridge component between the QML layer and the C++ core
 * (cppTimeline). Holds no clip state itself — all logic (clip model,
 * operations on clips) lives in C++ (Timeline). This component only:
 *   1) provides a thin addClip() wrapper for UI calls, with debug logging;
 *   2) subscribes to cppTimeline signals for debug output.
 * Actual UI updates (Timeline.qml) happen automatically through bindings
 * to the C++ model — this component never pushes updates manually.
 */
Item {
    id: clipManager
    visible: false // purely logical component, no visual representation

    /**
     * Add a clip to the timeline.
     * All logic (validation, insertion into the model, duration recalculation)
     * is delegated to C++ (cppTimeline.addClip). This function is a thin
     * wrapper with debug logging.
     *
     * @param {string} filepath    path to the media file
     * @param {int} trackNumber    track index (0 or 1)
     * @param {real} startTime     clip start position on the timeline, seconds
     */
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

        // No manual update needed: C++ emits clipsChanged() itself,
        // and every UI element bound to the model refreshes on its own
    }

    // Subscribes to C++ core signals — used only for debug console output;
    // the UI itself reacts to model changes directly through bindings,
    // independently of these handlers
    Connections {
        target: cppTimeline

        function onClipsChanged() {
            if (DEBUG_MODE)
            console.log(" Получен сигнал: clipsChanged от C++")
            // Timeline.qml updates automatically (bound to the model)
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
