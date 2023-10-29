import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    default property alias content: mainLayout.children
    ScrollView {
        height: mainLayout.i
        anchors.fill: parent;
        anchors.margins: 20
        ColumnLayout {
            id: mainLayout
            anchors.fill: parent;
        }
    }
}
