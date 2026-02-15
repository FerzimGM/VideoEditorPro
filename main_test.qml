import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    visible: true
    width: 800
    height: 600
    title: "VideoEditor Pro - TEST"
    color: "#1b2838"

    Rectangle {
        anchors.fill: parent
        color: "#1b2838"
        
        Column {
            anchors.centerIn: parent
            spacing: 20
            
            Text {
                text: "VideoEditor Pro"
                color: "#c7254e"
                font.pixelSize: 32
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Text {
                text: "Если ты видишь этот текст - Qt работает!"
                color: "#c7d5e0"
                font.pixelSize: 16
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Button {
                text: "Тестовая кнопка"
                anchors.horizontalCenter: parent.horizontalCenter
                
                contentItem: Text {
                    text: parent.text
                    color: "#c7d5e0"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: parent.pressed ? "#1e3a52" : (parent.hovered ? "#355a7a" : "#2a475e")
                    radius: 4
                    border.color: "#c7254e"
                    border.width: 1
                }
                
                onClicked: {
                    console.log("Кнопка работает!")
                    testText.text = "Кнопка нажата! ✓"
                }
            }
            
            Text {
                id: testText
                text: "Нажми кнопку выше"
                color: "#8f98a0"
                font.pixelSize: 14
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
