import QtQuick
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts

Item {
    id: root;
    property alias from: slider.from;
    property alias to: slider.to;
    property alias stepSize: slider.stepSize;
    property alias value: slider.value;
    property alias snapMode: slider.snapMode;
    signal moved();

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Label {
            id: label;
            Layout.preferredWidth: 30
            text: Math.round(slider.value * 100) / 100;
            font.underline: true;
        }
        Slider {
            id: slider;
            Layout.preferredWidth: root.width - 30
            onMoved: root.moved()
        }
    }
    height: slider.height;
    Layout.fillWidth: true;
}
