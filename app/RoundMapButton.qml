import QtQuick
import QtQuick.Controls.Material

Rectangle {
    property alias checkable: button.checkable
    property alias checked: button.checked
    signal clicked()
    property string icon_source: ""
    width: 46
    height: 46
    radius: width
    color: Material.backgroundColor
    RoundButton {
        id: button
        width: 60
        height: 60
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
