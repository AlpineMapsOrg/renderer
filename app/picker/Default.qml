import QtQuick
import QtQuick.Controls.Material
import app

Rectangle {
    id: root
    color: "red"
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
        wrapMode: Text.WordWrap
    }

    Label {
        id: type
        padding: 10
        anchors {
            top: title.bottom
            left: root.left
            right: root.right
        }
        text: "type: " + feature_properties.type;
        font.pixelSize: 14
        font.bold: true
        wrapMode: Text.WordWrap
    }

    ListView {
       id: feature_details_view
       anchors {
           top: type.bottom
           left: root.left
           right: root.right
           bottom: root.bottom
           margins: 10
       }
       model: Object.keys(feature_properties).map((key) => { return { key: key, value: feature_properties[key] }; });

       delegate: ItemDelegate {
           width: root.width
           height: 20

           Text{
               font.pixelSize: 14
               text: "<b>" + modelData.key + "</b>: " + modelData.value
               wrapMode: Text.Wrap
           }

           onClicked: {
               if(modelData.value.startsWith("http"))
               {
                    Qt.openUrlExternally(modelData.value);
               }
               else if(modelData.key === "phone")
               {
                    Qt.openUrlExternally("tel:" + modelData.value);
               }
               else if(modelData.key === "wikipedia")
               {
                   Qt.openUrlExternally("https://" + modelData.value.split(":")[0] + ".wikipedia.org/wiki/" + modelData.value.split(":")[1]);
               }
               else if(modelData.key === "wikidata")
               {
                   Qt.openUrlExternally("https://www.wikidata.org/wiki/" + modelData.value);
               }
           }
       }
    }
}
