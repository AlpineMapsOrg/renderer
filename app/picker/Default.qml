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
       model: feature_data_list
       delegate: ItemDelegate {
           width: root.width
           height: ((model.index % 2) == 0) ? 17 : 22

           Text{
               font.pixelSize: ((model.index % 2) == 0) ? 14 : 11
               font.bold: (model.index % 2) == 0
               text: feature_data_list[model.index]
               wrapMode: Text.Wrap

               color: {
                   if(feature_data_list[model.index].startsWith("http")
                    || ((model.index % 2 == 1) && feature_data_list[model.index-1] == "Phone:")
                    || ((model.index % 2 == 1) && feature_data_list[model.index-1] == "Wikipedia:")
                    || ((model.index % 2 == 1) && feature_data_list[model.index-1] == "Wikidata:")
                   )
                   {
                       return "#1b75d0"
                   }
                   else
                   {
                       return "#000000"
                   }
               }
           }

           onClicked: {
               if(feature_data_list[model.index].startsWith("http"))
               {
                    Qt.openUrlExternally(feature_data_list[model.index]);
               }
               else if(((model.index % 2 == 1) && feature_data_list[model.index-1] == "Phone:"))
               {
                    Qt.openUrlExternally("tel:" + feature_data_list[model.index]);
               }
               else if((model.index % 2 == 1) && feature_data_list[model.index-1] == "Wikipedia:")
               {
                   Qt.openUrlExternally("https://" + feature_data_list[model.index].split(":")[0] + ".wikipedia.org/wiki/" + feature_data_list[model.index].split(":")[1]);
               }
               else if((model.index % 2 == 1) && feature_data_list[model.index-1] == "Wikidata:")
               {
                   Qt.openUrlExternally("https://www.wikidata.org/wiki/" + feature_data_list[model.index]);
               }

           }
       }
    }
}
