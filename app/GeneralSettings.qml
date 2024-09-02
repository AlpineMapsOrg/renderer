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
import app
import "components"

SettingsPanel {
    Component.onCompleted: {
        responsive_update()
    }

    CheckGroup {
        name: qsTr("Date and Time")
        id: datetimegroup
        property date date;
        ModelBinding on date { target: map.settings; property: "datetime"; }

        Label { text: qsTr("Date:") }
        DatePicker {
            id: currentDate;
            selectedDate: datetimegroup.date
            onEditingFinished: {
                let copy = currentDate.selectedDate
                copy.setHours(datetimegroup.date.getHours());
                copy.setMinutes(datetimegroup.date.getMinutes());
                datetimegroup.date = copy;
            }
        }

        Label { text: qsTr("Time:") }
        LabledSlider {
            id: currentTime;
            from: 0.0; to: 24.0; stepSize: 1 / (60 / 15); // in 15 min steps
            value: datetimegroup.date.getHours() + datetimegroup.date.getMinutes() / 60

            onMoved: {
                let h = parseInt(value);
                datetimegroup.date.setHours(h);
                datetimegroup.date.setMinutes(parseInt((value - h) * 60))
            }
            formatCallback: function (value) {
                let h = parseInt(value);
                let m = parseInt((value - h) * 60);
                return  String(h).padStart(2, '0') + ":" + String(m).padStart(2, '0');
            }
        }

        Label { text: qsTr("Sun Angles:"); }
        Label {
            text: "Az(" + map.sun_angles.x.toFixed(2) + "°) , Ze(" + map.sun_angles.y.toFixed(2) + "°)"
        }

    }

    CheckGroup {
        name: qsTr("Camera")
        Label { text: qsTr("Field of view:") }
        LabledSlider {
            id: fov_slider;
            from: 15; to: 120; stepSize: 1;
            ModelBinding on value { target: map; property: "field_of_view"; }
        }

        Label { text: qsTr("Frame limiter:") }
        LabledSlider {
            id: frame_rate_slider;
            from: 10; to: 120; stepSize: 1;
            ModelBinding on value { target: map; property: "frame_limit"; }
        }

        Label { text: qsTr("Level of detail:") }
        LabledSlider {
            id: lod_slider;
            from: 0.1; to: 2.0; stepSize: 0.1;
            ModelBinding on value { target: map.settings; property: "render_quality"; }
        }
    }

    CheckGroup {
        name: qsTr("Cache & Network")

        Label { text: qsTr("Cache size:") }
        LabledSlider {
            id: cache_size_slider;
            from: 1000; to: 20000; stepSize: 1000;
            ModelBinding on value { target: map; property: "tile_cache_size"; }
        }

    }
}
