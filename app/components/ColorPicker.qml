import QtQuick
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root;

    property color color;
    property bool alphaEnabled: true;

    onColorChanged: {
        label.text = color.toString().toUpperCase();
        slider.value = color.a;
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
            Layout.fillWidth: !alphaEnabled;
            text: "#FFFFFFF";
            font.underline: true;

            MouseArea{
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    colorDialog.selectedColor = root.color;
                    colorDialog.open();
                }
            }
        }
        Slider {
            id: slider;
            from: 0.0; to: 1.0; stepSize: 0.01;
            Layout.fillWidth: true;
            visible: alphaEnabled;
            onValueChanged: color.a = value;
            implicitHeight: label.implicitHeight;
        }
    }

    ColorDialog {
        id: colorDialog
        options: alphaEnabled ? ColorDialog.ShowAlphaChannel : 0
        onAccepted: root.color = selectedColor
    }
    height: label.height;
    Layout.fillWidth: true;

}
