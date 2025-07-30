/*****************************************************************************
 * AlpineMaps.org
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

import QtQuick 6.4
import QtQuick.Controls.Material
import QtQuick.Layouts
import app
import "components"
import QtQuick.Controls 6.4

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
                FloatingActionButton {
                    image: _r + "icons/material/steepness.png"
                    onClicked: {map.shared_config.overlay_mode = 101; toggleSteepnessLegend();}
                    size: parent.height
                    image_size: 24
                    image_opacity: 1.0

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Overlay steepness")
                }
            }
        }
    }

    FloatingActionButton {
        id: avalanche_menu
        image: _r + "icons/" + (checked ? "material/chevron_left.png": "eaws/eaws_menu.png")
        size: parent.width
        checkable: true
        onClicked:{map.updateEawsReportDate(date_picker.selectedDate.getDate(), date_picker.selectedDate.getMonth()+1, date_picker.selectedDate.getYear()+1900)}

        Rectangle {
            visible: parent.checked
            height: 64
            width: avalanche_subgroup.implicitWidth + parent.width
            radius: avalanche_menu.radius

            color: Qt.alpha(Material.backgroundColor, 0.3)
            border { width: 2; color: Qt.alpha( "black", 0.5); }

            RowLayout {
                x: avalanche_menu.width
                y: parent.y
                id: avalanche_subgroup
                spacing: 0
                height: parent.height

                //EAWS Report Toggle Button
                FloatingActionButton {
                    id: eaws_report_toggle
                    image: _r + "icons/eaws/eaws_report.png"
                    image_opacity: (checked? 1.0 : 0.4)
                    onClicked:{
                        risk_level_toggle.checked = false;
                        slope_angle_toggle.checked  = false;
                        stop_or_go_toggle.checked = false;
                        banner_image.source = "eaws/banner_eaws_report.png"
                        map.toggle_eaws_warning_layer();
                    }
                    size: parent.height
                    image_size: 42
                    checkable: true

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Show EAWS Report")

                }

                // Risk Level Toggle Button
                FloatingActionButton {
                    id: risk_level_toggle
                    image: _r + "icons/eaws/risk_level.png"
                    onClicked:{
                        eaws_report_toggle.checked = false;
                        slope_angle_toggle.checked = false;
                        stop_or_go_toggle.checked = false;
                        banner_image.source = "eaws/banner_risk_level.png"
                        map.toggle_risk_level_layer();
                    }
                    size: parent.height
                    image_size: 42
                    image_opacity: (checked? 1.0 : 0.4)
                    checkable: true

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Risk Level")
                }

                // Slope Angle Toggle Button
                FloatingActionButton {
                    id: slope_angle_toggle
                    image: _r + "icons/eaws/slope_angle.png"

                    onClicked:{
                        eaws_report_toggle.checked = false;
                        risk_level_toggle.checked = false;
                        stop_or_go_toggle.checked = false;
                        banner_image.source = "eaws/banner_slope_angle.png"
                        map.toggle_slope_angle_layer();
                    }
                    size: parent.height
                    image_size: 42
                    image_opacity: (checked? 1.0 : 0.4)
                    checkable: true

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Slope Angle")
                }

                // stop-or-go toggle button
                FloatingActionButton {
                    id: stop_or_go_toggle
                    image: _r + "icons/eaws/stop_or_go.png"
                    onClicked:{
                        eaws_report_toggle.checked = false;
                        risk_level_toggle.checked = false;
                        slope_angle_toggle.checked  = false;
                        banner_image.source = "eaws/banner_stop_or_go.png"
                        map.toggle_stop_or_go_layer();
                    }
                    size: parent.height
                    image_size: 42
                    image_opacity: (checked? 1.0 : 0.4)
                    checkable: true

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Stop or Go")
                }

                // Banner with color chart (only visible when an avalanche overlay is active
                Image{
                    id: banner_image
                    Layout.preferredWidth: implicitWidth * Layout.preferredHeight / implicitHeight + 20
                    Layout.preferredHeight: 64
                    fillMode: Image.PreserveAspectFit  // Keep aspect ratio
                    visible: (eaws_report_toggle.checked || risk_level_toggle.checked || slope_angle_toggle.checked || stop_or_go_toggle.checked)
                }

                // Date picker for EAWS Report with confirm button
                DatePicker {
                    id: date_picker
                    selectedDate: new Date(2024, 11, 29) //new Date() for today
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth:  implicitWidth * Layout.preferredHeight / implicitHeight + 20
                    Layout.preferredHeight: 60
                    onSelectedDateChanged: {map.updateEawsReportDate(selectedDate.getDate(), selectedDate.getMonth()+1, selectedDate.getFullYear())}
                    // Note: month starts at 0
                }

                // link to selected date avalanche report
                Text {
                    id: externalLink
                    text: "<a href=\"" + formattedUrl + "\">Open on Avalanche.report</a>"
                    color: "black"
                    linkColor: "black"
                    font.pixelSize: 14
                    textFormat: Text.RichText
                    onLinkActivated: function(url) {Qt.openUrlExternally(url)}

                    property string formattedUrl: {
                        let date = date_picker.selectedDate;
                        let year = date.getFullYear();
                        let month = (date.getMonth() + 1).toString().padStart(2, "0");
                        let day = date.getDate().toString().padStart(2, "0");
                        return "https://avalanche.report/bulletin/" + year + "-" + month + "-" + day;
                    }
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




