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
import QtQuick.Controls.Material
import QtQuick.Layouts
import Alpine

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

    Image {
        function oc_scale() : float {
            if (renderer.camera_operation_centre_distance < 0) {
                return 1.0;
            }
            let max_dist = 1000.0;
            let scale = 1 + Math.pow((1 - (Math.min(max_dist, renderer.camera_operation_centre_distance) / max_dist)) * 1.6, 6);
            return scale;
        }
        id: camera_operation_centre
        source: "icons/camera_operation_centre.svg"
        width: 16 * oc_scale()
        height: 16 * oc_scale()
        sourceSize: Qt.size(width, height)
        x: renderer.camera_operation_centre.x - width / 2
        y: renderer.camera_operation_centre.y - 60 - height / 2
        visible: renderer.camera_operation_centre_visibility && punkt.checked
    }

    Repeater {
        id: label_view
        anchors.fill: parent

        model: CameraTransformationProxyModel {
            sourceModel: LabelModel {}
            camera: renderer.camera
        }

        delegate: Rectangle {
            id: delegate_root
            property int horizontal_text_margin: 40
            property int vertical_text_margin: 10
            property real alpha_value: my_alpha()
            visible: alpha_value > 0
            function my_scale() : float {
                let importance_scale = model.importance / 10;
                let distance_scale = model.size + 0.3
                return importance_scale * (importance_scale + 0.5) * distance_scale
            }
            function my_alpha() : float {
                let alpha = my_scale();
                if (alpha < 0.37)
                    alpha = 0.0;
                else if (alpha < 0.47)
                    alpha = (alpha - 0.37) / 0.1;
                else if (alpha > 3)
                    alpha = 1 - (Math.min(alpha, 4) - 3);
                else
                    alpha = 1;
                return Math.min(alpha, 0.7);
            }
            x: model.x * label_view.width / renderer.camera_width
            y: model.y * (label_view.height + 60) / renderer.camera_height - 60
            z:  50 * my_scale()
            Image {
                id: icon
                source: "icons/peak.svg"
                width: 16 * my_scale()
                height: 16 * my_scale()
                sourceSize: Qt.size(width, height)
                x: -width/2
                y: -height
                anchors.centerIn: parent
            }

            Rectangle {
                id: text_rect
                x: -(width) / 2
                y: -icon.height - 20 * my_scale() - (height) / 2
                color: Qt.alpha(Material.backgroundColor, delegate_root.alpha_value)
                width: label_text.width + horizontal_text_margin
                height: label_text.height + vertical_text_margin
                scale: my_scale()
                radius: height
                Text {
                    anchors.centerIn: parent
                    id: label_text
                    color: Qt.alpha(Material.primaryTextColor, delegate_root.alpha_value)
                    text: model.text + "(" + model.altitude + "m)"
                    font.pixelSize: 25
                }
            }

        }
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
        icon_source: "icons/compass.svg"
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
        icon_source: "icons/current_location.svg"
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
}
