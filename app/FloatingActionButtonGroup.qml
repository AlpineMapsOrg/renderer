/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

ColumnLayout {
    id: fab_group
    property real button_spacing: 0
    property real group_margin: 15
    width: 64
    x: group_margin
    y: map.height - implicitHeight - group_margin
    spacing: button_spacing

    function responsive_update() {
        let newVisible = true;
        if (main_stack_view.selectedPage === "settings") {
            if (main.width < main.height) newVisible = false;
        } else if (main_stack_view.selectedPage !== "map") {
            newVisible = false;
        }
        fab_group.visible = newVisible;
    }

    Connections {
        target: main
        function onWidthChanged() {
            responsive_update();
        }
        function onHeightChanged() {
            responsive_update();
        }
    }
    Connections {
        target: main_stack_view;
        function onSelectedPageChanged() {
            responsive_update();
        }
    }

    FloatingActionButton {
        rotation: map.camera_rotation_from_north
        image: "../icons/material/navigation_offset.svg"
        onClicked: map.rotate_north()
        size: parent.width
    }

    FloatingActionButton {
        id: fab_location
        image: checked ?  "../icons/material/my_location.svg" : "../icons/material/location_searching.svg"
        checkable: true
        size: parent.width
    }

    FloatingActionButton {
        id: fab_presets
        image: checked ? "../icons/material/chevron_left.svg" : "../icons/material/format_paint.svg"
        size: parent.width
        checkable: true

        Rectangle {
            visible: parent.checked
            radius: parent.radius
            height: 64
            width: fabsubgroup.implicitWidth + parent.width

            color: Qt.alpha(Material.backgroundColor, 0.3)
            border { width:2; color:Qt.alpha( "black", 0.5); }

            RowLayout {
                x: parent.parent.width
                id: fabsubgroup
                spacing: 0
                height: parent.height

                FloatingActionButton {
                    image: "../icons/presets/basic.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP303YEDIPrZPr0FQHr_EU-HBAYEwKn_5syZIPX2DxgEGLDQcP0_ILQDBwMKcHBgwAoc7KC0CJTuhyh0yGRAoeHueIBK4wAKQMwIxXAAAFQuIIw")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("BASIC-Preset: Fast with no shading")
                }

                FloatingActionButton {
                    image: "../icons/presets/shaded.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP1s0rwEMG32D0TvPxS4yIEBAXDqvzlz5gIQ_YBBgAELDdf_A0I7cDCgAAcHBqzAwQ5Ki0DpfohCh0wGFBrujgeoNBAwQjEyXwFNHEwDAMaIIAM")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("SHADED-Preset: Shading + SSAO + CSM")
                }

                FloatingActionButton {
                    image: "../icons/presets/snow.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP1s0rwEMG32D0TvPxS4yIEBAXDqvzlz5gIQ_YBBgAELDdf_A0I7cDCgAAcHVPPg4nZQWgRK90MUOmQyoNBwdzxApYGAEYqR-Qpo4mAaAFhrITI")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("SNOW-Preset: Shading + SSAO + CSM + Snow Layer")
                }
            }
        }
    }

    Connections {
        enabled: fab_location.checked || fab_presets.checked
        target: map
        function onMouse_pressed() {
            fab_location.checked = false;
            fab_presets.checked = false;
        }

        function onTouch_made() {
            fab_location.checked = false;
            fab_presets.checked = false;
        }
    }

    GnssInformation {
        id: gnss
        enabled: fab_location.checked
        onInformation_updated: {
            map.set_position(gnss.latitude, gnss.longitude)
        }
    }

}


