/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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

Rectangle {
    id: filterMenu

    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 2*innerMargin

    color:  Qt.alpha(Material.backgroundColor, 0.7)

    // similar to StatsWindow.qml
    function responsive_update() {
        x = 8 + 64 + 8 // FAB(FloatingActionButtonGroup) margin + FAB width + FAB margin
        if (map.width >= map.height) {
            y = map.height - main_content.height - 8-64-8
            maxHeight = main.height - tool_bar.height - 20
            height = main_content.height
            width = 300
        } else {
            // usually on mobile devices (portrait mode)
            y = map.height - main_content.height - 8-64-8
            height = main_content.height
            maxHeight = main.height - tool_bar.height - 20
            width = main.width - 2 * x

        }
    }

    Component.onCompleted: responsive_update()

    Connections {
        target: main
        function onWidthChanged() {
            responsive_update();
        }
        function onHeightChanged() {
            responsive_update();
        }
    }

    SettingsPanel {
        id: main_content
        maxHeight: filterMenu.maxHeight // needs to be set directly because parents height is dependent
        anchors {
            left: parent.left
            right: parent.right
        }

        SettingsTitle { title: "Filter" }

        CheckGroup {
            id: filter_peaks
            name: "Peaks"
            checkBoxEnabled: true
            ModelBinding on checked { target: map; property: "label_filter.peaks_visible"; }

            Label {
                text: "Elevation"
            }

            LabledRangeSlider {
                id: filter_peaks_elevation_range;
                from: 0; to: 4000; stepSize: 10;
                labelWidth:100;
                ModelBinding on first.value { target: map; property: "label_filter.peak_ele_range.x"; }
                ModelBinding on second.value { target: map; property: "label_filter.peak_ele_range.y"; }
            }

            CheckBox{
                ModelBinding on checked { target: map; property: "label_filter.peak_has_cross"; }
            }
            Label {
                text: "has cross"
            }

            CheckBox{
                ModelBinding on checked { target: map; property: "label_filter.peak_has_register"; }
            }
            Label {
                text: "has register"
            }

        }

        CheckGroup {
            id: filter_cities
            name: "Cities"
            checkBoxEnabled: true
            ModelBinding on checked { target: map; property: "label_filter.cities_visible"; }
        }

        CheckGroup {
            id: filter_cottages
            name: "Cottages"
            checkBoxEnabled: true
            ModelBinding on checked { target: map; property: "label_filter.cottages_visible"; }
            CheckBox{
                ModelBinding on checked { target: map; property: "label_filter.cottage_has_shower"; }
            }
            Label {
                text: "has shower"
            }

            CheckBox{
                ModelBinding on checked { target: map; property: "label_filter.cottage_has_contact"; }
            }
            Label {
                text: "has contact"
            }
        }

        CheckGroup {
            id: filter_webcams
            name: "Webcams"
            checkBoxEnabled: true
            ModelBinding on checked { target: map; property: "label_filter.webcams_visible"; }
            Label {
                text: " " // padding, without the tickbox doesn't work propertly
            }
        }
    }
}


