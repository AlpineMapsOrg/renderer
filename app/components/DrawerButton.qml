import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Alpine

Item {
    id: root
    property alias iconSource: icon.source
    property alias text: label.text
    property int innerPadding: 14
    property bool selectable: true
    property int bid: {
        return Math.floor(Math.random() * 100000);
    }

    property variant drawer: parent.parentDrawer

    signal clicked()

    Layout.fillWidth: true
    height: rowlayout.implicitHeight + 2 * innerPadding
    Component.onCompleted: evaluateBackgroundColor(false)

    function evaluateBackgroundColor(hover) {
        if (hover) {
            background.color = Qt.alpha(Material.accentColor, 0.3)
        } else {
            if (drawer.selectedButtonId === bid) {
                background.color = Qt.alpha(Material.backgroundDimColor, 0.2)
            } else {
                background.color = "transparent"
            }
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: "transparent"
    }

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.drawer.handleClick(root);
        onEntered: root.evaluateBackgroundColor(true);
        onExited: root.evaluateBackgroundColor(false);
    }

    RowLayout {
        id: rowlayout
        spacing: innerPadding
        anchors.margins: innerPadding
        anchors.fill: parent

        Image {
            id: icon
            smooth: true
            opacity: 0.54
            fillMode: Image.Pad
            sourceSize: Qt.size (24, 24)
            verticalAlignment: Image.AlignVCenter
            horizontalAlignment: Image.AlignHCenter
        }

        Item {
            width: 36 - (2 * spacing)
        }

        Label {
            id: label
            opacity: 0.87
            font.pixelSize: 14
            text: qsTr("Map")
            Layout.fillWidth: true
            font.weight: Font.Medium
        }
    }
}
