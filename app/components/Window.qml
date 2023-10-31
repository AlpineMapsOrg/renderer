import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts


Rectangle {
    id: root

    default property alias content: main_content.children
    property variant unhandledContent: ({});

    width: 270
    height: main_content.height
    x: 10
    y: parent.height / 2 - height / 2
    color:  Qt.alpha(Material.backgroundColor, 0.7)

    SetPanel {
        id: main_content
        maxHeight: map.height // needs to be set directly because parents height is dependent

        anchors {
            left: parent.left
            right: parent.right
        }

    }
}
