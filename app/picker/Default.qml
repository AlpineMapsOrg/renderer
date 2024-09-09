/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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
           height: text.height

           Label {
               id: text
               textFormat: Text.MarkdownText
               text: "**" + modelData.key + "**: " + modelData.value
               wrapMode: Text.Wrap
               width: root.width - 20
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
