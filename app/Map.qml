/*****************************************************************************
 * AlpineMaps.org
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
import app
import "components"

Rectangle {
    id: map_gui
    color: "#00000000"
    property TerrainRenderer renderer

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
        border { width: 2; color:Qt.alpha( "black", 0.5); }
        radius: 16 * oc_scale()
        visible: renderer.camera_operation_centre_visibility
    }

    PickerWindow {
        id: feature_detail_window_loader
        map: renderer
    }

    FloatingActionButtonGroup {
        id: fab_group
        anchors {
            left: map_gui.left
            bottom: map_gui.bottom
            leftMargin: 8
            bottomMargin: 8
        }
    }

    Rectangle {
        id: copyright_frame
        // property string originalText: qsTr("© [OpenStreetMap](https://www.openstreetmap.org/copyright), © [basemap.at](https://basemap.at/)")
        property string originalText: qsTr("© <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a>, © <a href=\"https://basemap.at/\">basemap.at</a>")
        property string placeholderText: qsTr("©")

        anchors {
            bottom: map_gui.bottom
            right: map_gui.right
        }
        width: copyright.width + 16
        height: copyright.height + 4
        color: Qt.alpha(Material.backgroundColor, 0.7)
        Behavior on width { NumberAnimation { duration: 1000; easing.type: Easing.OutExpo } }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true

            onClicked: {
                delayTimer.stop()
                copyright.text = copyright_frame.originalText
            }

            onExited: {
                // Restart the timer when mouse leaves
                delayTimer.restart()
            }
        }

        Timer {
            id: delayTimer
            interval: 8000 // 8 seconds delay
            running: true
            repeat: false
            onTriggered: {
                copyright.text = copyright_frame.placeholderText
            }
        }

        Label {
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 8
            }
            id: copyright
            onLinkActivated: Qt.openUrlExternally(link)
            textFormat: Text.StyledText

            text: copyright_frame.originalText
        }
    }

}
