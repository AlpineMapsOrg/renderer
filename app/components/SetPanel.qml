import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    default property alias content: mainLayout.children
    property int maxHeight: parent.height
    property int innerPadding: 20

    height: {
        let newHeight = mainLayout.implicitHeight + 2 * innerPadding;
        return newHeight > maxHeight ? maxHeight : newHeight;
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: innerPadding
        implicitHeight: mainLayout.implicitHeight

        //padding: innerPadding
        ColumnLayout {
            id: mainLayout
            anchors.left: parent.left;
            anchors.right: parent.right;
        }
    }
}
