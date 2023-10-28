import QtQuick
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root;
    property color color;
    /*
    property alias from: slider.from;
    property alias to: slider.to;
    property alias stepSize: slider.stepSize;
    property alias value: slider.value;
    property alias snapMode: slider.snapMode;
    signal moved();*/

    onColorChanged: {
        label.text = color.toString().toUpperCase();
    }

    RowLayout {
        anchors.fill: parent
        spacing: 5
        Rectangle {
            Layout.preferredHeight: parent.height - 10;
            Layout.preferredWidth: parent.height - 10;
            color: root.color;
            border { width:1; color:Qt.alpha( "white", 1.0); }
        }
        Label {
            id: label;
            padding: 5;
            Layout.fillWidth: true;
            text: "#FFFFFFF";
            font.underline: true;
        }
    }
    MouseArea{
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            colorDialog.selectedColor = root.color;
            colorDialog.open();
        }
    }
    ColorDialog {
        id: colorDialog
        options: ColorDialog.ShowAlphaChannel
        onAccepted: root.color = selectedColor
    }
    height: label.height;
    Layout.fillWidth: true;

}
