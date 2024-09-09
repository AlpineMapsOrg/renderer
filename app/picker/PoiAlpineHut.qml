import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import app

Rectangle {
    id: root
    Label {
        id: title
        padding: 10
        anchors {
            top: root.top
            left: root.left
            right: root.right
        }
        text: feature_title;
        font.pixelSize: 18
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
    Flickable {
        anchors {
            margins: 10
            top: title.bottom
            left: root.left
            right: root.right
        }
        GridLayout {
            anchors.fill: parent
            columns: 2
            Label {
                text: "Coordinates: "
                font.bold: true
            }
            TextEdit {
                text: feature_properties.latitude.toFixed(5) + " / " + feature_properties.longitude.toFixed(5)
            }
            Label {
                text: "Altitude:"
                font.bold: true
            }
            Label {
                text: feature_properties.altitude.toFixed(0)
            }
            Label {
                text: ""
                font.bold: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            Column {
                id: links
                Layout.fillWidth: true
                Button {
                    text: "Call: " + feature_properties.phone
                    visible: typeof feature_properties.phone !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally("tel:" + feature_properties.phone);
                    }
                }
                Button {
                    text: "Web: " + feature_properties.website
                    visible: typeof feature_properties.website !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally(feature_properties.website);
                    }
                }
            }
        }
    }

}
