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

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "components"
import app

Item {
    id: main
    property int theme: Material.Light
    property int accent: Material.BlueGrey
    property string selectedPage: "map"
    property bool userInterfaceVisible: true
    property real safeTop: SafeArea.margins.top
    property real safeLeft: SafeArea.margins.left
    property real safeRight: SafeArea.margins.right
    property real safeBottom: SafeArea.margins.bottom

    Shortcut {
        id: ui_visibility_shortcut
        sequence: "F10"
        onActivated: userInterfaceVisible = !userInterfaceVisible
        context: Qt.ApplicationShortcut
    }
    Shortcut {
        id: reload_shaders_shortcut
        sequence: "F6"
        onActivated: map.reload_shader()
        context: Qt.ApplicationShortcut
    }

    Rectangle {
        id: tool_bar
        height: 60
        color: main_stack_view.depth === 1 ? "#01FF00FF" : Qt.alpha(Material.backgroundColor, 0.7)
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: main.safeLeft
            rightMargin: main.safeRight
            topMargin: main.safeTop
        }
        visible: userInterfaceVisible

        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            Rectangle {
                width: 48
                height: 48
                color: "#00FF0000"
                Image {
                    source: _r + "icons/menu.png"
                    width: parent.width / 2
                    height: parent.height / 2
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        menu.open()
                        search_results.visible = false
                    }
                }
            }
            Label {
                id: page_title
                text: ""
                wrapMode: Label.Wrap
                font.pointSize: 24
                Layout.fillWidth: true
            }
            SearchBox {
                id: search
                search_results: search_results
            }
        }
        z: 100
    }

    SearchResults {
        id: search_results
        map: map
        search_height: search.height
        anchors {
            top: tool_bar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            topMargin: 10
            leftMargin: 10 + main.safeLeft
            rightMargin: 10 + main.safeRight
            bottomMargin: 10 + main.safeBottom
        }
    }

    PageDrawer {
        id: menu
        selectedButtonId: 0

        DrawerSeparator {}

        DrawerButton {
            bid: 0
            text: qsTr ("Map")
            iconSource: _r + "icons/material/map.png"
            onClicked: change_page("map", qsTr("Map"))
        }

        DrawerButton {
            text: qsTr ("Coordinates")
            iconSource: _r + "icons/material/pin_drop.png"
            onClicked: change_page("Coordinates.qml", qsTr("Coordinates"))
        }

        DrawerButton {
            text: qsTr ("GPX Editor")
            iconSource: _r + "icons/material/track.png"
            onClicked: change_page("GpxEditor.qml", qsTr("GPX Editor"))
        }

        DrawerButton {
            text: qsTr ("Settings")
            iconSource: _r + "icons/material/settings.png"
            onClicked: change_page("Settings.qml", qsTr("Settings"))
        }

        DrawerSeparator {}

        DrawerButton {
            text: qsTr("Reload Shaders")
            iconSource: _r + "icons/material/3d_rotation.png"
            hotkey: reload_shaders_shortcut.nativeText
            selectable: false
            onClicked: map.reload_shader();
        }

        DrawerButton {
            text: (stats_window_loader.item !== null && stats_window_loader.item.visible) ? qsTr ("Hide Statistics") : qsTr("Statistics")
            hotkey: "F8"
            iconSource: _r + "icons/material/monitoring.png"
            selectable: false
            visible: _debug_gui
            onClicked: toggleStatsWindow();
        }

        DrawerButton {
            text: qsTr("Hide User Interface")
            iconSource: _r + "icons/material/visibility_off.png"
            hotkey: ui_visibility_shortcut.nativeText
            selectable: false
            onClicked: userInterfaceVisible = !userInterfaceVisible
        }

        DrawerSpacer {}

        DrawerSeparator {}

        DrawerButton {
            text: qsTr ("About")
            iconSource: _r + "icons/material/info.png"
            onClicked: change_page("About.qml", qsTr("About"))
        }

    }

    function change_page(source, title) {
        selectedPage = source.toLowerCase().replace(".qml", "");
        if (selectedPage !== "map" && selectedPage !== "settings"  && stats_window_loader.item !== null) {
            stats_window_loader.item.visible = false;
        }
        // the gpx editor only accepts clicks on the map while its page is open
        map.change_edit_mode(selectedPage === "gpxeditor");
        if (source === "map") {
            if (main_stack_view.depth >= 1) main_stack_view.pop()
            page_title.visible = false
            search.visible = true
            main_stack_view.selectedPage = selectedPage
            return
        }
        if (main_stack_view.depth === 1)
            main_stack_view.push(source)
        else
            main_stack_view.replace(source)
        // the gpx editor's close button returns to the map page
        if (main_stack_view.currentItem.closeRequested !== undefined)
            main_stack_view.currentItem.closeRequested.connect(function() { change_page("map", qsTr("Map")) })
        main_stack_view.selectedPage = selectedPage
        page_title.visible = true
        search.visible = false
        page_title.text = title
        main.onWidthChanged(); // trigger responsive updates manually
    }

    function toggleStatsWindow() {
        if (stats_window_loader.item === null)
            stats_window_loader.source = "StatsWindow.qml"
        else
            stats_window_loader.item.visible = !stats_window_loader.item.visible
        main.onWidthChanged(); // trigger responsive updates manually
    }

    function toggleFilterWindow() {
        if (filter_window_loader.item === null)
            filter_window_loader.source = "FilterWindow.qml"
        else
            filter_window_loader.item.visible = !filter_window_loader.item.visible
        main.onWidthChanged(); // trigger responsive updates manually
    }

    function toggleSteepnessLegend() {
        steepnessLegend.visible = !steepnessLegend.visible
    }

    TerrainRenderer {
        id: map
        focus: true
        anchors.fill: parent
        Keys.onPressed: function(event){
            if (event.key === Qt.Key_F8 && _debug_gui) {
                toggleStatsWindow();
            }
            else if(event.key === Qt.Key_F7)
            {
                toggleFilterWindow();
            }
        }
        Behavior on field_of_view { NumberAnimation { duration: 500; easing.type: Easing.InOutExpo } }
    }

    // route points picked in the gpx editor (map clicks) land in the planner model
    Connections {
        target: map
        function onPlanner_point_picked(latitude, longitude, altitude, screenX, screenY) {
            _planner_model.add_point_at(latitude, longitude, altitude, screenX, screenY)
        }
        // hold-to-drag of route points
        function onPlanner_press_geo(latitude, longitude) {
            _planner_model.press_at(latitude, longitude)
        }
        function onPlanner_move_geo(latitude, longitude, altitude) {
            _planner_model.move_dragged(latitude, longitude, altitude)
        }
        function onPlanner_release() {
            _planner_model.release_at()
        }
        // the fly bar buttons must not keep the keyboard focus: the arrow
        // keys steer the preview (left/right seek, up/down speed)
        function onPreview_state_changed() {
            // follow the actual preview speed wherever it came from
            // (dropdown pick or arrow keys)
            speed_combo.currentIndex = speed_combo.index_for(map.preview_speed)
            if (map.preview_active)
                map.forceActiveFocus()
        }
    }

    // signals of the planner model itself (not the map): the waypoint
    // insertion request and the route point drag state
    Connections {
        target: _planner_model
        // a click landed on the planned route: let the editor ask whether a
        // waypoint should be created there
        function onWaypoint_insertion_requested(latitude, longitude, altitude, afterIndex, screenX, screenY) {
            if (main.selectedPage === "gpxeditor" && main_stack_view.currentItem !== null
                && main_stack_view.currentItem.request_waypoint_insertion !== undefined) {
                main_stack_view.currentItem.request_waypoint_insertion(latitude, longitude, altitude, afterIndex, screenX, screenY)
            }
        }
        function onRoute_point_dragging_changed(dragging) {
            map.planner_drag_active = dragging
        }
        // a route was imported or opened: frame it whole, keeping the camera
        // orientation as far as possible and above the dem
        function onRoute_framing_requested(minLat, minLon, maxLat, maxLon, minAlt, maxAlt) {
            map.frame_area(minLat, minLon, maxLat, maxLon, minAlt, maxAlt)
        }
    }

    // route preview controls, floating above the map while a fly-through runs.
    // sized to also fit phone screens; controls take no focus so the arrow
    // keys keep steering the preview
    Rectangle {
        id: preview_controls
        visible: map.preview_active
        color: Qt.alpha(Material.backgroundColor, 0.85)
        radius: 8
        height: 56
        clip: true
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: 8 + main.safeLeft
            rightMargin: 8 + main.safeRight
            bottomMargin: 12 + main.safeBottom
        }
        z: 150

        RowLayout {
            id: preview_row
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            ToolButton {
                focusPolicy: Qt.NoFocus
                // shows Play at the route end (auto-paused), and Play while
                // the user paused - pressing it resumes or restarts
                text: map.preview_paused ? qsTr("Play") : qsTr("Pause")
                onClicked: map.set_preview_paused(!map.preview_paused)
                ToolTip.visible: hovered
                ToolTip.text: map.preview_paused ? qsTr("Resume the preview") : qsTr("Pause the preview")
            }
            Slider {
                id: preview_progress_slider
                focusPolicy: Qt.NoFocus
                from: 0
                to: 1
                value: map.preview_progress
                onMoved: map.seek_preview(value)
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                ToolTip.visible: hovered
                ToolTip.text: Math.round(value * 100) + " %"
            }
            ComboBox {
                id: speed_combo
                focusPolicy: Qt.NoFocus
                model: [qsTr("Slow"), qsTr("Normal"), qsTr("Fast"), qsTr("Very fast")]
                // sized to fit inside the bar (the default combo background
                // is taller than the tool buttons and poked out below)
                implicitHeight: 36
                currentIndex: 1 // normal
                onActivated: map.set_preview_speed(speed_combo.speed_for(index))
                function speed_for(index) { return [12, 24, 48, 96][index]; }
                function index_for(speed) {
                    const speeds = [12, 24, 48, 96];
                    let best = 1;
                    for (let i = 0; i < speeds.length; ++i) {
                        if (Math.abs(speeds[i] - speed) < Math.abs(speeds[best] - speed))
                            best = i;
                    }
                    return best;
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Flight speed")
            }
            ToolButton {
                focusPolicy: Qt.NoFocus
                text: "×"
                onClicked: map.stop_route_preview()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("End the route preview")
            }
        }
    }

    StackView {
        property string selectedPage: "map"
        id: main_stack_view
        anchors {
            top: tool_bar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            leftMargin: main.safeLeft
            rightMargin: main.safeRight
            bottomMargin: main.safeBottom
        }
        visible: userInterfaceVisible

        initialItem: Map {
            renderer: map
        }
    }

    Loader {
        id: stats_window_loader
    }

    Loader {
        id: filter_window_loader
    }

    Component.onCompleted: {
        change_page("map")
    }

    Rectangle {
        id: steepnessLegend
        width: 300
        height: 48
        color: Qt.alpha( "white", 0.7)
        border.color: Qt.alpha( "black", 0.5)
        border.width: 2
        radius: 8   // Optional: Rounded corners
        anchors.left: parent.left
        anchors.leftMargin: main.safeLeft
        anchors.top: tool_bar.bottom
        anchors.topMargin: 4
        visible: false

        Image {
            id: steepnessLegendImage
            anchors.fill: parent
            source: "icons/steepness_legend.png"   // Local file or resource
            fillMode: Image.PreserveAspectFit
            smooth: true
            anchors.margins: 4
        }
    }

}
