/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celerek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
import QtQuick.Layouts
import Alpine

import "components"

Rectangle {
    id: map_gui
    color: "#00000000"
    property TerrainRenderer renderer

    GnssInformation {
        id: gnss
        enabled: current_location.checked
        onInformation_updated: {
            renderer.set_position(gnss.latitude, gnss.longitude)
        }
    }

    Rectangle {
        function oc_scale() : real {
            if (renderer.camera_operation_centre_distance < 0) {
                return 1.0;
            }
            let max_dist = 1000.0;
            let scale = 1 + Math.pow((1 - (Math.min(max_dist, renderer.camera_operation_centre_distance) / max_dist)) * 1.6, 6);
            return scale;
        }
        width: 16 * oc_scale()
        height: 16 * oc_scale()
        x: renderer.camera_operation_centre.x - width / 2
        y: renderer.camera_operation_centre.y - 60 - height / 2
        color: Qt.alpha(Material.backgroundColor, 0.7);
        border { width:2; color:Qt.alpha( "black", 0.5); }
        radius: 16 * oc_scale()
        visible: renderer.camera_operation_centre_visibility && punkt.checked
    }

    RoundButton {
        id: punkt
        width: 60
        height: 60
        checkable: true
        checked: true
        focusPolicy: Qt.NoFocus
        text: "punkt"
        visible: false
        anchors {
            right: parent.right
            bottom: compass.top
            rightMargin: 10
            bottomMargin: 10
        }
    }
    RoundMapButton {
        id: compass
        rotation: renderer.camera_rotation_from_north
        icon_source: "../icons/compass.svg"
        onClicked: renderer.rotate_north()

        anchors {
            right: parent.right
            bottom: current_location.top
            margins: 16
        }
    }

    RoundMapButton {
        id: current_location
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 16
        }
        checkable: true
        icon_source: "../icons/current_location.svg"
    }

    Connections {
        enabled: current_location.checked
        target: renderer
        function onMouse_pressed() {
            current_location.checked = false;
        }

        function onTouch_made() {
            current_location.checked = false;
        }
    }

    Connections {
        target: map
        function onHud_visible_changed(hud_visible) {
            current_location.visible = hud_visible;
            compass.visible = hud_visible;
        }
    }
}
