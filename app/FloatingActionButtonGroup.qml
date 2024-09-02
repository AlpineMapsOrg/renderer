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
import app
import "components"

ColumnLayout {
    id: fab_group
    width: 64
    spacing: 0

    FloatingActionButton {
        rotation: map.camera_rotation_from_north
        image: _r + "icons/material/navigation_offset.png"
        onClicked: map.rotate_north()
        size: parent.width
    }

    FloatingActionButton {
        id: fab_location
        image: _r + "icons/material/" + (checked ? "my_location.png" : "location_searching.png")
        checkable: true
        size: parent.width
    }

    FloatingActionButton {
        image: _r + "icons/material/add.png"
        size: parent.width
        onClicked: {
            _track_model.upload_track()
            let pos = _track_model.lat_long(_track_model.n_tracks() - 1);
            if (pos.x !== 0 && pos.y !== 0)
                map.set_position(pos.x, pos.y)
        }
    }

    FloatingActionButton {
        id: fab_presets
        image: _r + "icons/material/" + (checked ? "chevron_left.png" : "format_paint.png")
        size: parent.width
        checkable: true

        Rectangle {
            visible: parent.checked
            radius: parent.radius
            height: 64
            width: fabsubgroup.implicitWidth + parent.width

            color: Qt.alpha(Material.backgroundColor, 0.3)
            border { width: 2; color: Qt.alpha( "black", 0.5); }

            RowLayout {
                x: parent.parent.width
                id: fabsubgroup
                spacing: 0
                height: parent.height

                FloatingActionButton {
                    image: _r + "icons/presets/basic.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP303YEDIPrZPr0FQHr_EU-HBAYEwKn_5syZIPX2DxgEGLDQcP0_ILQDBwMKcHBgwAoc7KC0CJTuhyh0yGRAoeHueIBK4wAKQMwIxXAAAFQuIIw")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("BASIC-Preset: Fast with no shading")
                }

                FloatingActionButton {
                    image: _r + "icons/presets/shaded.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP1s0rwEMG32D0TvPxS4yIEBAXDqvzlz5gIQ_YBBgAELDdf_A0I7cDCgAAcHBqzAwQ5Ki0DpfohCh0wGFBrujgeoNBAwQjEyXwFNHEwDAMaIIAM")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("SHADED-Preset: Shading + SSAO + CSM")
                }

                FloatingActionButton {
                    image: _r + "icons/presets/snow.png"
                    onClicked: map.set_gl_preset("AAABIHjaY2BgYLL_wAAGGPRhY2EHEP1s0rwEMG32D0TvPxS4yIEBAXDqvzlz5gIQ_YBBgAELDdf_A0I7cDCgAAcHVPPg4nZQWgRK90MUOmQyoNBwdzxApYGAEYqR-Qpo4mAaAFhrITI")
                    size: parent.height
                    image_size: 42
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("SNOW-Preset: Shading + SSAO + CSM + Snow Layer")
                }

                FloatingActionButton {
                    image: _r + "icons/material/filter_alt.png"
                    onClicked: toggleFilterWindow();
                    size: parent.height
                    image_size: 24
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Filter visible labels")
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


