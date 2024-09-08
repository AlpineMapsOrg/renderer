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
import "picker"

Rectangle {
    property TerrainRenderer map
    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 20
    property int maxWidth:  map.width - 100
    // readonly property var available_picker_types: {"PoiPeak", "PoiWebcam"}

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
    function source_file_for(pick_type) {
        if (!map.current_feature_data.is_valid())
            return ""
        if (typeof pick_type === "undefined")
            return "picker/Default.qml"
        let mySet = {};
        mySet["PoiPeak"] = true;
        if (pick_type in mySet)
            return "picker/" + map.current_feature_data.properties.type + ".qml"

        return "picker/Default.qml"
    }

    Loader {
        // sourceComponent: poiDefault
        anchors.fill: parent
        // source: map.current_feature_data.is_valid() ? "picker/" + map.current_feature_data.properties.type + ".qml" : ""
        source: source_file_for(map.current_feature_data.properties.type)
        property string feature_title: map.current_feature_data.title
        property var feature_properties: map.current_feature_data.properties
        // onStatusChanged: {
        //     if (status == Loader.Error) {
        //         source: "picker/Default.qml"
        //     }

        //     console.log("loader status: " + status)
        // }
    }
}
