import QtQuick
import QtQuick.Controls
import "../theme.js" as Theme

/**
 * SplashScreen
 * ------------
 * Loading screen shown at application startup. Progress is not a real
 * indicator of C++ core initialization — it's simulated via a Timer that
 * steps through a predefined list of status strings. Once the list is
 * exhausted, the loaded() signal fires and the parent is expected to
 * switch to the main interface.
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: Theme.backgroundColor

    property int progress: 0                 // current load progress, 0..100
    property string status: "Загрузка..."    // current stage text, shown under the title

    signal loaded() // emitted once, when the simulated loading sequence finishes

    Column {
        anchors.centerIn: parent
        spacing: 30

        // Логотип/Иконка
        Text {
            text: "▶"
            color: Theme.rubyPrimary
            font.pixelSize: 120
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Название
        Text {
            text: "VideoEditor Pro"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 32
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Статус загрузки
        Text {
            text: root.status
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 14
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Прогресс бар
        Rectangle {
            width: 300
            height: 4
            color: Theme.backgroundDark
            radius: 2
            anchors.horizontalCenter: parent.horizontalCenter

            // Filled portion of the bar — width is proportional to progress
            // (0..100); the width animation smooths out each Timer step
            Rectangle {
                width: parent.width * (root.progress / 100)
                height: parent.height
                color: Theme.rubyPrimary
                radius: 2

                Behavior on width {
                    NumberAnimation { duration: 200 }
                }
            }
        }

        // Крутящийся индикатор
        BusyIndicator {
            width: 40
            height: 40
            running: true
            anchors.horizontalCenter: parent.horizontalCenter

            // Custom contentItem instead of the default BusyIndicator look:
            // a single dot rotating around the indicator's center
            contentItem: Item {
                implicitWidth: 40
                implicitHeight: 40

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.rubyPrimary
                    x: parent.width / 2 - 3
                    y: 0

                    transform: Rotation {
                        origin.x: 3
                        origin.y: 20
                        angle: 0

                        NumberAnimation on angle {
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                        }
                    }
                }
            }
        }

        // Версия
        Text {
            text: "v0.0.1"
            color: Theme.textDisabled
            font.family: Theme.fontFamily
            font.pixelSize: 12
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Timer that simulates step-by-step loading: every 100 ms it advances
    // status/progress to the next entry in steps. The list is a purely
    // visual simulation — there's no actual synchronization with C++
    // core initialization here
    Timer {
        id: loadTimer
        interval: 100
        repeat: true
        running: true

        property var steps: [
            "Инициализация...",
            "Загрузка темы...",
            "Загрузка панели меню...",
            "Загрузка панели эффектов...",
            "Загрузка видеоплеера...",
            "Загрузка таймлайна...",
            "Готово!"
        ]
        property int currentStep: 0

        onTriggered: {
            if (currentStep < steps.length) {
                root.status = steps[currentStep]
                root.progress = (currentStep / steps.length) * 100
                currentStep++
            } else {
                loadTimer.stop()
                // Give a bit more time for full initialization: callLater
                // defers the signal to the next event loop cycle so the UI
                // has a chance to render the final state (100%, "Готово!")
                Qt.callLater(function() {
                    root.loaded()
                })
            }
        }
    }
}
