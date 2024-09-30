/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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
import QtQuick.Layouts
import QtQuick.Controls.Material
import app

Rectangle {
    property TerrainRenderer map
    property double search_height
    property alias model: search_results_view.model
    id: search_results
    visible: false
    anchors {
        top: tool_bar.bottom
        bottom: root_window.contentItem.bottom
        left: root_window.contentItem.left
        right: root_window.contentItem.right
        margins: 10
    }
    color: "#AAFFFFFF"
    radius: search_height / 2
    RoundButton {
        anchors {
            top: parent.top
            right: parent.right
        }
        text: "X"
        width: search_height
        height: search_height
        z: 110
        onClicked: search_results.visible = false
    }

    ListView {
        id: search_results_view
        anchors.fill: parent
        model: ListModel {}
        delegate: ItemDelegate {
                width: search_results_view.width
                text: model.properties.display_name
                font.pixelSize: 20
                onClicked: {
//                        console.log(model.geometry.coordinates[1] + "/" + model.geometry.coordinates[0])
                    map.set_position(model.geometry.coordinates[1], model.geometry.coordinates[0])
                    search_results.visible = false
                }
        }
    }
    z: 100
}
