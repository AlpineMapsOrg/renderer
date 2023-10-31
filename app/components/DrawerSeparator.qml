import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Alpine

Item {
    id: root
    Layout.fillWidth: true
    height: 1

    Rectangle {
        id: background
        anchors.fill: parent
        color: Qt.alpha(Material.backgroundDimColor, 0.3)
    }

}
