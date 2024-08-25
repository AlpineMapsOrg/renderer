﻿/*****************************************************************************
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
import QtCharts
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

import "components"

Rectangle {
    property TerrainRenderer map
    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 2*innerMargin

    id: featureDetailMenu

    visible: map.current_feature_data.is_valid()

    z: 100
    color:  Qt.alpha(Material.backgroundColor, 0.9)
    anchors {
        bottom: parent.bottom
        right: parent.right
        margins: 10
    }


    // similar to StatsWindow.qml
    function responsive_update() {
        if (map.width >= map.height) {
            maxHeight = main.height - tool_bar.height - 20
            height = main_content.height
            width = 300;
        } else {
            // usually on mobile devices (portrait mode)
            height = main_content.height
            maxHeight = main.height - tool_bar.height - 20
            width = main.width - 2 * x

        }
    }


    Component.onCompleted: responsive_update()

    Connections {
        target: main
        function onWidthChanged() {
            responsive_update();
        }
        function onHeightChanged() {
            responsive_update();
        }
    }


    Item {
        id: main_content
        height:  (300 > maxHeight) ? maxHeight : 300

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
                       if(map.current_feature_data_list[model.index].startsWith("http"))
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
               }
           }
       }


    }







}

