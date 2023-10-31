import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    property alias title: label.text

    Layout.fillWidth: true
    Layout.preferredHeight: label.implicitHeight

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(Material.backgroundDimColor, 0.2)
    }
    Label {
        id: label
        padding: 10
        font.capitalization: Font.AllUppercase
        text: "Unset Title"
        font.pixelSize: 16
        font.bold: true
        anchors.left: parent.left
        anchors.right: parent.right
        horizontalAlignment: Text.AlignHCenter
    }
}
