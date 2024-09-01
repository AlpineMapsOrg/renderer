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

Rectangle {
    property SearchResults search_results

    id: search
    color: "#AAFFFFFF"
    radius: 100
    Layout.fillWidth: true
    height: 48
    TextField {
        anchors {
            fill: parent
            leftMargin: 6
            rightMargin: 6
        }
        id: search_input
        placeholderText: (focus || text.length) ? "" : "City or Mountain.."
        background: Rectangle{ color: "#00FFFFFF" }
        verticalAlignment: TextInput.AlignVCenter
        visible: parent.visible
        font.pointSize: 20
        font.family: "Source Sans 3"
        onAccepted: {
            console.log("onAccepted")
            if (text.length <= 2)
                return
            var xhr = new XMLHttpRequest
            xhr.onreadystatechange = function() {
                console.log("xhr.onreadystatechange")
                if (xhr.readyState === XMLHttpRequest.DONE) {
                    console.log("xhr.readyState === XMLHttpRequest.DONE")
                    var responseJSON = JSON.parse(xhr.responseText)
                    search_results.model.clear()
                    var feature_array = responseJSON.features  //JSONPath.jsonPath(responseJSON, ".features")
                    for ( var index in feature_array ) {
                        var jo = feature_array[index];
                        search_results.model.append(jo);
                    }

                    search_results.visible = true
                }
            }
            xhr.open("GET", encodeURI("https://nominatim.openstreetmap.org/search?q=" + text + "&limit=5&countrycodes=at&format=geojson"))
            xhr.send()
        }
    }
    Button {
        id: search_button
        anchors {
            top: search_input.top
            bottom: search_input.bottom
            right: search_input.right
        }

        text: ""
        icon.source: _r + "icons/search.png"
        background: Rectangle { color: "#00FFFFFF" }
        onClicked: {
            search_input.accepted()
        }
    }

}
