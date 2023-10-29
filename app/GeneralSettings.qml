/*****************************************************************************
 * Alpine Terrain Renderer
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

SetPanel {
    Component.onCompleted: {
        // when creating the this component, values are read from the renderer
        // after that we establish a binding, so this component can set values on the renderer
        frame_rate_slider.value = map.frame_limit
        lod_slider.value = map.render_quality
        fov_slider.value = map.field_of_view
        cache_size_slider.value = map.tile_cache_size
        map.frame_limit = Qt.binding(function() { return frame_rate_slider.value })
        map.render_quality = Qt.binding(function() { return lod_slider.value })
        map.field_of_view = Qt.binding(function() { return fov_slider.value })
        map.tile_cache_size = Qt.binding(function() { return cache_size_slider.value })
        responsive_update()
    }

    SetGroup {
        name: qsTr("Camera")
        Label { text: qsTr("Field of view:") }
        ValSlider {
                    id: fov_slider;
                    from: 15; to: 120; stepSize: 1;
                }

                Label { text: qsTr("Frame limiter:") }
                ValSlider {
                    id: frame_rate_slider;
                    from: 2; to: 120; stepSize: 1;
                }

                Label { text: qsTr("Level of detail:") }
                ValSlider {
                    id: lod_slider;
                    from: 0.1; to: 2.0; stepSize: 0.1;
                }
            }

            SetGroup {
                name: qsTr("Cache & Network")

                Label { text: qsTr("Cache size:") }
                ValSlider {
                    id: cache_size_slider;
                    from: 1000; to: 20000; stepSize: 1000;
                }

                Label {
                    text: qsTr("Cachedd:")
                }
                ProgressBar {
                    Layout.fillWidth: true
                    id: cache_fill_slider
                    from: 0
                    to: cache_size_slider.value
                    value: map.cached_tiles
                }

                Label {
                    text: qsTr("Queued:")
                }
                ProgressBar {
                    Layout.fillWidth: true
                    id: queued_slider
                    from: 0
                    to: 200
                    value: map.queued_tiles
                }
            }
}
