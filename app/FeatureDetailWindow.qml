/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

import QtQuick
import QtQuick.Controls.Material
import app
import "components"

Rectangle {
    property TerrainRenderer map
    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 20
    property int maxWidth:  map.width - 100

    id: featureDetailMenu

    visible: map.current_feature_data.is_valid()

    height:  (300 > maxHeight) ? maxHeight : 300
    width:  (300 > maxWidth) ? maxWidth : 300

    z: 100
    color:  Qt.alpha(Material.backgroundColor, 0.9)
    anchors {
        bottom: parent.bottom
        right: parent.right
        margins: 10
    }

    Item {
        id: main_content

        height: parent.height
        width: parent.width

        anchors {
            left: parent.left
            right: parent.right
        }

        Label {
            id: title
            padding: 10
            text: map.current_feature_data.title;
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        ListView {

           id: feature_details_view
           anchors {
               top: title.bottom
               left: parent.left
               right: parent.right
               bottom: parent.bottom
               margins: 10
           }
           model: map.current_feature_data_list
           delegate: ItemDelegate {
               width: parent.width
               height: ((model.index % 2) == 0) ? 17 : 22

               Text{
                   font.pixelSize: ((model.index % 2) == 0) ? 14 : 11
                   font.bold: (model.index % 2) == 0
                   text:map.current_feature_data_list[model.index]
                   wrapMode: Text.Wrap

                   color: {
                       if(map.current_feature_data_list[model.index].startsWith("http")
                        || ((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Phone:")
                        || ((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Wikipedia:")
                        || ((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Wikidata:")
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
                   if(map.current_feature_data_list[model.index].startsWith("http"))
                   {
                        Qt.openUrlExternally(map.current_feature_data_list[model.index]);
                   }
                   else if(((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Phone:"))
                   {
                        Qt.openUrlExternally("tel:" + map.current_feature_data_list[model.index]);
                   }
                   else if((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Wikipedia:")
                   {
                       Qt.openUrlExternally("https://" + map.current_feature_data_list[model.index].split(":")[0] + ".wikipedia.org/wiki/" + map.current_feature_data_list[model.index].split(":")[1]);
                   }
                   else if((model.index % 2 == 1) && map.current_feature_data_list[model.index-1] == "Wikidata:")
                   {
                       Qt.openUrlExternally("https://www.wikidata.org/wiki/" + map.current_feature_data_list[model.index]);
                   }

               }
           }
       }
    }
}
