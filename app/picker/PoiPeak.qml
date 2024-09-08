import QtQuick
import QtQuick.Controls.Material
import app

Rectangle {
    id: root
    Label {
        padding: 10
        anchors {
            top: root.top
            left: root.left
            right: root.right
        }
        text: feature_title + " (" + feature_properties.ele + "m)";
        font.pixelSize: 18
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
