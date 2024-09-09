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
        text: feature_title + ((typeof feature_properties.population !== "undefined") ? " (pop: " + feature_properties.population + ")" : "");
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
                text: "Links:"
                font.bold: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            Column {
                id: links
                Layout.fillWidth: true
                Button {
                    text: "Wikipedia"
                    visible: typeof feature_properties.wikipedia !== "undefined"
                    width: parent.width
                    onClicked: {
                        let link_split = feature_properties.wikipedia.split(":")
                        Qt.openUrlExternally("https://" + link_split[0] + ".wikipedia.org/wiki/" + link_split[1]);
                    }
                }
                Button {
                    text: "Wikidata"
                    visible: typeof feature_properties.wikidata !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally("https://www.wikidata.org/wiki/" + feature_properties.wikidata);
                    }
                }
                Button {
                    text: "Homepage"
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
