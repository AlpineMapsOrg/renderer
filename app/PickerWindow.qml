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

import app
import "components"
import "picker"

Rectangle {
    property TerrainRenderer map
    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 20
    property int maxWidth:  map.width - 100

    id: featureDetailMenu

    visible: map.picked_feature.is_valid()

    height:  (300 > maxHeight) ? maxHeight : 300
    width:  (300 > maxWidth) ? maxWidth : 300

    z: 100
    color: Qt.alpha(Material.backgroundColor, 0.7)
    anchors {
        bottom: parent.bottom
        right: parent.right
        margins: 10
    }
    function source_file_for(pick_type) {
        if (typeof pick_type === "undefined")
            return "picker/Default.qml"
        let mySet = {};
        mySet["PoiPeak"] = true;
        mySet["PoiWebcam"] = true;
        mySet["PoiAlpineHut"] = true;
        mySet["PoiSettlement"] = true;
        if (pick_type in mySet)
            return "picker/" + pick_type + ".qml"

        return "picker/Default.qml"
    }

    Loader {
        anchors.fill: parent
        source: source_file_for(map.picked_feature.properties.type)
        property string feature_title: map.picked_feature.title
        property var feature_properties: map.picked_feature.properties
    }
}
