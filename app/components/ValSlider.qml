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
    property var formatCallback: defaultFormatCallback;
    signal moved();

    function defaultFormatCallback(value) {
        return Math.round(slider.value * 100) / 100;
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Slider {
            id: slider;
            Layout.preferredWidth: root.width - 50
            onMoved: root.moved()
        }
        Label {
            id: label;
            Layout.preferredWidth: 50
            text: {
                return root.formatCallback(root.value);
            }
            font.underline: true;
        }
    }
    height: slider.implicitHeight;
    Layout.fillWidth: true;
}
