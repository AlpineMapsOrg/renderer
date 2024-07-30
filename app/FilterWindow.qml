/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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
import QtCharts
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

import "components"

Rectangle {
    id: filterMenu

    property int innerMargin: 10
    property int maxHeight:  main.height - tool_bar.height - 2*innerMargin

    color:  Qt.alpha(Material.backgroundColor, 0.7)

    // TODO position it better (maybe consider integrating this more into FAB (nesting it in there to better utilize the anchors/margins from there)
    // similar to StatsWindow.qml
    function responsive_update() {
        x = 8 + 64 + 8 // FAB(FloatingActionButtonGroup) margin + FAB width + FAB margin
        if (map.width >= map.height) {
            y = map.height - main_content.height - 8-64-8
//            y = tool_bar.height + innerMargin
            maxHeight = main.height - tool_bar.height - 20
            height = main_content.height
            width = 300
        } else {
            // usually on mobile devices (portrait mode)
            if (main.selectedPage === "settings") {
                y = tool_bar.height + innerMargin
                height = main.height - main.height / 2.0 - tool_bar.height - 2 * innerMargin
                maxHeight = height
                width = main.width - 2 * innerMargin
            } else {
                y = parseInt(main.height / 2.0)
                height = main.height / 2.0
                maxHeight = height
                width = main.width - 2 * innerMargin
            }
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
            checked: label_filter.peaks_visible

            onCheckedChanged: label_filter.peaks_visible = filter_peaks.checked;

            Label {
                Layout.columnSpan: 2
                text: "Elevation"
            }

            LabledRangeSlider {
                Layout.columnSpan: 2
                id: filter_peaks_elevation_range;
                from: 0; to: 4000; stepSize: 10;
                first.value: 0; second.value: 4000;
                labelWidth:100;
                first.onMoved: label_filter.elevation_range.x = this.first.value;
                second.onMoved: label_filter.elevation_range.y = this.second.value;
            }

            Button{
                text: "Filter"
                onClicked: label_filter.trigger_filter_update()
            }
        }

        CheckGroup {
            id: filter_cities
            name: "Cities"
            checkBoxEnabled: true
            checked: label_filter.cities_visible

            onCheckedChanged: label_filter.cities_visible = filter_cities.checked;
        }

        CheckGroup {
            id: filter_cottages
            name: "Cottages"
            checkBoxEnabled: true
            checked: label_filter.cottages_visible

            onCheckedChanged: label_filter.cottages_visible = filter_cottages.checked;
        }




//        CheckGroup {
////            id: filter_cities
//            name: "Cities"
//            checkBoxEnabled: true
//            checked: true
////            onClicked: label_filter.filter_updated()
//        }

//        Connections {
//            target: filter_cities
//            onCheckChanged: console.log("cities changed")
//        }
    }
}


