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

SettingsPanel {
    Component.onCompleted: {
        // when creating the this component, values are read from the renderer
        // after that we establish a binding, so this component can set values on the renderer
        frame_rate_slider.value = map.frame_limit
        lod_slider.value = map.settings.render_quality
        fov_slider.value = map.field_of_view
        cache_size_slider.value = map.tile_cache_size

        map.frame_limit = Qt.binding(function() { return frame_rate_slider.value })
        map.settings.render_quality = Qt.binding(function() { return lod_slider.value })
        map.field_of_view = Qt.binding(function() { return fov_slider.value })
        map.tile_cache_size = Qt.binding(function() { return cache_size_slider.value })
        datetimegroup.initializePropertys();
        responsive_update()
    }

    CheckGroup {
        name: qsTr("Date and Time")
        id: datetimegroup

        property bool initialized: false;

        function initializePropertys() {
            var jdt = new Date(map.settings.datetime);
            currentTime.value = jdt.getHours() + jdt.getMinutes() / 60;
            currentDate.selectedDate = jdt;
            initialized = true;
        }

        function updateMapDateTimeProperty() {
            if (!initialized) return;
            let jsDate = currentDate.selectedDate;
            jsDate.setHours(currentTime.hours);
            jsDate.setMinutes(currentTime.minutes);
            map.settings.datetime = jsDate;
        }

        Label { text: qsTr("Date:") }
        DatePicker {
            id: currentDate;
            onSelectedDateChanged: {
                datetimegroup.updateMapDateTimeProperty();
            }
        }

        Label { text: qsTr("Time:") }
        LabledSlider {
            property int hours;
            property int minutes;
            id: currentTime;
            from: 0.0; to: 24.0; stepSize: 1 / (60 / 15); // in 15 min steps
            onMoved: {
                datetimegroup.updateMapDateTimeProperty();
            }
            formatCallback: function (value) {
                let h = parseInt(value);
                hours = h;
                let m = parseInt((value - h) * 60);
                minutes = m;
                return  String(h).padStart(2, '0') + ":" + String(m).padStart(2, '0');
            }
        }

        CheckBox {
            id: link_gl_settings;
            text: "Link GL Sun Configuration"
            Layout.fillWidth: true;
            Layout.columnSpan: 2;
            checked: map.settings.gl_sundir_date_link;
            onCheckStateChanged: map.settings.gl_sundir_date_link = this.checked;
        }

        Label { text: qsTr("Sun Angles:"); visible:  map.settings.gl_sundir_date_link; }
        Label {
            visible: map.settings.gl_sundir_date_link;
            text: {
                return "Az(" + map.sun_angles.x.toFixed(2) + "°) , Ze(" + map.sun_angles.y.toFixed(2) + "°)";
            }
        }

    }

    CheckGroup {
        name: qsTr("Camera")
        Label { text: qsTr("Field of view:") }
        LabledSlider {
                    id: fov_slider;
                    from: 15; to: 120; stepSize: 1;
                }

                Label { text: qsTr("Frame limiter:") }
                LabledSlider {
                    id: frame_rate_slider;
                    from: 2; to: 120; stepSize: 1;
                }

                Label { text: qsTr("Level of detail:") }
                LabledSlider {
                    id: lod_slider;
                    from: 0.1; to: 2.0; stepSize: 0.1;
                }
            }

            CheckGroup {
                name: qsTr("Cache & Network")

                Label { text: qsTr("Cache size:") }
                LabledSlider {
                    id: cache_size_slider;
                    from: 1000; to: 20000; stepSize: 1000;
                }

            }
}
