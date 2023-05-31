import QtQuick
import QtQuick.Controls.Material

Rectangle {
    property alias checkable: button.checkable
    property alias checked: button.checked
    property alias rotation: button.rotation
    signal clicked()
    property string icon_source: ""
    width: 50
    height: 50
    radius: width
    color: button.checked ? Material.accentColor : Material.backgroundColor
    RoundButton {
        id: button
        width: parent.width + 10
        height: parent.height + 10
        highlighted: checked
        focusPolicy: Qt.NoFocus
        icon {
            source: icon_source
            height: 38
            width: 38
        }
        onClicked: parent.clicked()
        anchors.centerIn: parent
    }
}
