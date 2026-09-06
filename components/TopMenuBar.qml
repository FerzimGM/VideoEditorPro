import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme.js" as Theme

/**
 * TopMenuBar
 * ----------
 * Custom window title bar (used instead of the native one — the app
 * apparently runs as a frameless window). Combines two roles:
 *   1) the application's main menu (File/Project/Export) — only emits
 *      signals outward; the actual actions (opening a file, saving, etc.)
 *      are performed by the parent component;
 *   2) window controls (minimize/maximize/close) and a drag area for
 *      moving the window with the mouse, since there's no native title bar.
 * All actions merely emit signals — TopMenuBar itself never touches files
 * or knows any business-logic details.
 */
Rectangle {
    id: root
    color: Theme.panelBackground

    // Menu item signals — handled by the owner (main.qml)
    signal openVideo()
    signal openProject()
    signal saveProject()
    signal exportVideo()
    // Window control signals (needed since there's no native title bar)
    signal minimize()
    signal maximize()
    signal close()

    // Reference to the Window object — needed for manual window dragging
    // and maximizing, since a frameless window has no native title bar
    // that would handle this
    property var targetWindow: null

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rubyGradientStart }
            GradientStop { position: 1.0; color: Theme.rubyGradientEnd }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: Theme.spacing

        // Кнопка меню
        CustomButton {
            text: "Меню"
            icon: "☰"
            onClicked: {
                if (mainMenu.visible) {
                    mainMenu.close()
                } else {
                    mainMenu.popup(this, 0, height)
                }
            }
        }

        Text {
            text: "VideoEditor Pro"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }

        // Empty area used for dragging the window: substitutes for the
        // native title bar that's missing in frameless mode
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            MouseArea {
                anchors.fill: parent

                property point clickPos: Qt.point(0, 0)

                // Remember the press point relative to the MouseArea itself
                onPressed: (mouse) => {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }

                // Move the window by the cursor delta from the press point;
                // since clickPos is local to the MouseArea (not screen
                // coordinates), the window moves without the cursor
                // "jumping" relative to it
                onPositionChanged: (mouse) => {
                    if (pressed && root.targetWindow) {
                        var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                        root.targetWindow.x += delta.x
                        root.targetWindow.y += delta.y
                    }
                }

                // Double-click on the free area — like in regular windows,
                // maximizes/restores the window
                onDoubleClicked: {
                    if (root.targetWindow) {
                        root.maximize()
                    }
                }
            }
        }

        // Window control buttons (minimize / maximize / close) —
        // substitute for the native buttons missing on a frameless window
        RowLayout {
            Layout.fillHeight: true
            spacing: 0

            CustomButton {
                text: "−"
                onClicked: root.minimize()
            }

            CustomButton {
                text: "□"
                onClicked: root.maximize()
            }

            CustomButton {
                text: "×"
                onClicked: root.close()
            }
        }
    }

    // Application's main dropdown menu; opened via popup() from the "Меню"
    // button above. Background, item delegate and separators are all
    // customized to match the app's overall dark theme
    Menu {
        id: mainMenu
        width: 250

        background: Rectangle {
            color: Theme.panelBackground
            border.color: Theme.rubyPrimary
            border.width: 1
            radius: Theme.borderRadius
        }

        // Custom menu item delegate: label on the left, shortcut on the
        // right (pulled from the bound Action.shortcut, if set)
        delegate: MenuItem {
            id: menuItem
            implicitWidth: parent ? parent.width : 0
            implicitHeight: 35

            contentItem: RowLayout {
                spacing: Theme.spacingLarge

                Text {
                    text: menuItem.text
                    color: menuItem.highlighted ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    Layout.fillWidth: true
                }

                Text {
                    text: menuItem.action && menuItem.action.shortcut ? menuItem.action.shortcut : ""
                    color: Theme.textDisabled
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    visible: text !== ""
                }
            }

            background: Rectangle {
                color: menuItem.highlighted ? Theme.hoverColor : "transparent"
                radius: Theme.borderRadius
            }
        }

        Action {
            text: "Открыть видео"
            shortcut: "Ctrl+O"
            onTriggered: root.openVideo()
        }

        Action {
            text: "Открыть проект"
            shortcut: "Ctrl+Shift+O"
            onTriggered: root.openProject()
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }

        Action {
            text: "Сохранить проект"
            shortcut: "Ctrl+S"
            onTriggered: root.saveProject()
        }

        Action {
            text: "Сохранить как..."
            shortcut: "Ctrl+Shift+S"
            onTriggered: root.saveProject()
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }

        Action {
            text: "Экспорт видео..."
            shortcut: "Ctrl+E"
            onTriggered: root.exportVideo()
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }

        Action {
            text: "Выход"
            shortcut: "Alt+F4"
            onTriggered: root.close()
        }
    }

    // Local inline component for a title bar button (icon + text,
    // hover highlight). Reused both for the "Меню" button and for the
    // window control buttons below
    component CustomButton: Rectangle {
        property string text: ""
        property string icon: ""
        signal clicked()

        implicitWidth: contentRow.width + 16
        implicitHeight: 30
        color: mouseArea.containsMouse ? Theme.buttonHover : "transparent"
        radius: Theme.borderRadius

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: Theme.spacingSmall

            Text {
                text: icon
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLarge
                visible: icon !== ""
            }

            Text {
                text: parent.parent.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }
}
