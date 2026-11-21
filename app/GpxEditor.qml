/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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
import QtQuick.Dialogs

import app
import "components"

// the root is transparent and handles no mouse events, so clicks that miss
// the editor panel reach the map underneath (the map stays fully navigable)
Rectangle {
    id: gpx_root
    color: "#00FFFFFF"

    // emitted when the user closes the editor via the panel's x button
    signal closeRequested()

    // collapsed to a small status bar, so the map stays visible (phones)
    property bool minimized: false
    property bool preview_running: false

    // the editor opens on the saved-routes overview; the planner panel is
    // shown for a new, imported or loaded route
    property bool show_routes: true
    onShow_routesChanged: map.change_edit_mode(!show_routes)

    // optimisation weight sliders, collapsed behind the chevron by default
    property bool weights_visible: false

    function format_distance(d) {
        return d < 1000 ? d.toFixed(0) + " m" : (d / 1000).toFixed(2) + " km";
    }

    // pending waypoint insertion (map click on the route line): a small
    // button floats above the click position; any press outside of it
    // cancels without adding a point
    property bool waypoint_insertion_active: false
    property double pending_latitude: 0
    property double pending_longitude: 0
    property double pending_altitude: 0
    property int pending_after_index: -1

    function request_waypoint_insertion(latitude, longitude, altitude, after_index, screen_x, screen_y) {
        pending_latitude = latitude;
        pending_longitude = longitude;
        pending_altitude = altitude;
        pending_after_index = after_index;
        const button_width = waypoint_add_button.width > 0 ? waypoint_add_button.width : 110;
        const button_height = waypoint_add_button.height > 0 ? waypoint_add_button.height : 36;
        // high enough above the click position that a finger on a
        // touchscreen does not cover the button
        const gap = 60;
        waypoint_add_button.x = Math.max(8, Math.min(screen_x - button_width / 2, gpx_root.width - button_width - 8));
        waypoint_add_button.y = screen_y - button_height - gap;
        if (waypoint_add_button.y < 8) {
            waypoint_add_button.y = Math.min(screen_y + gap, gpx_root.height - button_height - 8);
        }
        waypoint_insertion_active = true;
    }

    function cancel_waypoint_insertion() {
        waypoint_insertion_active = false;
    }

    // leave the overview and open the planner panel; start minimized on
    // narrow screens (phone portrait), where the expanded panel would cover
    // the whole map
    function open_planner() {
        show_routes = false;
        minimized = gpx_root.width < 500;
    }

    // continue planning when re-opened with a route in progress; the routes
    // overview (which accepts no map clicks) only shows for an empty plan
    Component.onCompleted: {
        show_routes = _planner_model.point_count === 0;
        minimized = !show_routes && gpx_root.width < 500;
        map.change_edit_mode(!show_routes);
    }

    // react to the camera fly-through preview: hide the whole widget once on
    // start (not on every progress update), disable adding points, restore
    // everything when the preview ends
    Connections {
        target: map
        function onPreview_state_changed() {
            if (map.preview_active && !gpx_root.preview_running) {
                gpx_root.preview_running = true;
                gpx_root.visible = false;
                map.change_edit_mode(false);
            } else if (!map.preview_active && gpx_root.preview_running) {
                gpx_root.preview_running = false;
                gpx_root.visible = true;
                // only re-enable point picking if the editor page is still open
                if (main.selectedPage === "gpxeditor") {
                    map.change_edit_mode(!gpx_root.show_routes);
                }
            }
        }
    }

    // full-window press catcher: while the add-waypoint button is up, any
    // press outside of it cancels the insertion without adding a point
    MouseArea {
        visible: gpx_root.waypoint_insertion_active
        enabled: gpx_root.waypoint_insertion_active
        anchors.fill: parent
        z: 90
        onPressed: function(mouse) {
            gpx_root.cancel_waypoint_insertion();
            mouse.accepted = true;
        }
    }

    Button {
        id: waypoint_add_button
        visible: gpx_root.waypoint_insertion_active
        z: 91
        text: qsTr("Add waypoint")
        font.bold: true
        onClicked: {
            _planner_model.insert_waypoint(pending_latitude, pending_longitude, pending_altitude, pending_after_index);
            gpx_root.cancel_waypoint_insertion();
        }
    }

    FileDialog {
        id: export_dialog
        title: qsTr("Export GPX track")
        nameFilters: ["GPX (*.gpx)"]
        defaultSuffix: "gpx"
        fileMode: FileDialog.SaveFile
        onAccepted: _planner_model.export_gpx(export_dialog.selectedFile)
    }

    FileDialog {
        id: import_dialog
        title: qsTr("Import GPX track")
        nameFilters: ["GPX (*.gpx *.xml)"]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            // only leave the overview when the file actually parsed
            if (_planner_model.import_gpx(import_dialog.selectedFile) && gpx_root.show_routes) {
                gpx_root.open_planner()
            }
        }
    }

    Rectangle {
        id: gpx_frame
        color: Qt.alpha(Material.backgroundColor, 0.9)
        radius: 8
        clip: true
        anchors {
            left: gpx_root.left
            bottom: gpx_root.bottom
            leftMargin: 10
            bottomMargin: 10
        }
        width: Math.min(400, gpx_root.width - 20)
        height: minimized ? minimized_bar.implicitHeight + 16 : gpx_root.height - 20

        Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // saved routes overview: the editor's entry view
        ColumnLayout {
            id: routes_view
            visible: gpx_root.show_routes
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                Label {
                    text: qsTr("Saved Routes")
                    font.bold: true
                    font.pointSize: 16
                    Layout.fillWidth: true
                }
                // mirror of the planner's back button: switches to the
                // planner while a route is in progress
                DirectionButton {
                    visible: _planner_model.point_count > 0
                    direction: 180
                    onClicked: gpx_root.open_planner()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Continue editing the current route")
                }
                ToolButton {
                    text: "×"
                    onClicked: gpx_root.closeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Close the route planner")
                }
            }

            Label {
                visible: _planner_model.saved_routes.length === 0
                text: qsTr("No saved routes yet")
                opacity: 0.7
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            ListView {
                id: saved_routes_list
                model: _planner_model.saved_routes
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: saved_routes_list.contentHeight > saved_routes_list.height
                        ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                }

                delegate: RowLayout {
                    width: saved_routes_list.width

                    ItemDelegate {
                        Layout.fillWidth: true
                        onClicked: {
                            if (_planner_model.load_route(modelData.file)) {
                                gpx_root.open_planner()
                            }
                        }
                        contentItem: ColumnLayout {
                            spacing: 0
                            Label {
                                text: modelData.name
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.points + " points | " + format_distance(modelData.distance)
                                font.pointSize: 11
                                opacity: 0.7
                                elide: Text.ElideRight
                            }
                        }
                    }
                    ToolButton {
                        text: "×"
                        onClicked: _planner_model.delete_route(modelData.file)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Delete this route")
                    }
                }
            }

            Label {
                visible: _planner_model.status !== ""
                text: _planner_model.status
                wrapMode: Label.Wrap
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.bottomMargin: 10

                Button {
                    text: qsTr("Create New")
                    Layout.fillWidth: true
                    onClicked: {
                        _planner_model.reset()
                        gpx_root.open_planner()
                    }
                }
                Button {
                    text: qsTr("Import GPX")
                    Layout.fillWidth: true
                    onClicked: {
                        // on webassembly the model opens the browser file
                        // picker instead; only the desktop falls back to the
                        // native dialog
                        if (!_planner_model.import_route_file())
                            import_dialog.open()
                    }
                }
            }
        }

        // scrollable body: on small screens the content is taller than the
        // panel, and the buttons at the bottom must stay reachable
        Flickable {
            id: panel_scroll
            visible: !gpx_root.show_routes && !gpx_root.minimized
            anchors.fill: parent
            contentHeight: content_column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // flickables do not consume wheel events: without this the wheel
            // would propagate to the map and zoom it. the waypoint list has
            // its own handler and consumes the wheel first
            MouseArea {
                parent: panel_scroll
                anchors.fill: panel_scroll
                acceptedButtons: Qt.NoButton // clicks pass through
                onWheel: (wheel) => {
                    const max_y = Math.max(0, panel_scroll.contentHeight - panel_scroll.height);
                    panel_scroll.contentY = Math.max(0, Math.min(max_y, panel_scroll.contentY - wheel.angleDelta.y * 0.5));
                    wheel.accepted = true;
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: panel_scroll.contentHeight > panel_scroll.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
            }

            ColumnLayout {
                id: content_column
                width: panel_scroll.width
                spacing: 8

                RowLayout {
                    id: header_row
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    DirectionButton {
                        onClicked: gpx_root.show_routes = true
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Back to saved routes")
                    }
                    Label {
                        text: qsTr("Route Planner")
                        font.bold: true
                        font.pointSize: 16
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        id: help_button
                        text: "?"
                        checkable: true
                        checked: false
                        font.bold: checked
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("How to plan a route")
                    }
                    DirectionButton {
                        direction: minimized ? 90 : 270
                        onClicked: gpx_root.minimized = !gpx_root.minimized
                        ToolTip.visible: hovered
                        ToolTip.text: minimized ? qsTr("Show route planner") : qsTr("Minimize to see the map")
                    }
                    ToolButton {
                        text: "×"
                        onClicked: gpx_root.closeRequested()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Close the route planner")
                    }
                }

                Label {
                    visible: help_button.checked
                    text: qsTr("Click on the map to add route points. Dragging still moves the camera.")
                    wrapMode: Label.Wrap
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                Label {
                    text: _planner_model.point_count + " points | "
                          + format_distance(_planner_model.distance) + " | "
                          + _planner_model.ascent.toFixed(0) + " / "
                          + _planner_model.descent.toFixed(0) + " m"
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                Label {
                    text: qsTr("Steepness: avg ")
                          + _planner_model.avg_steepness.toFixed(1) + "° / max "
                          + _planner_model.max_steepness.toFixed(1) + "°"
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                ElevationProfile {
                    id: elevation_chart
                    profile: _planner_model.elevation_profile
                    // follow the flight through the profile
                    progress: map.preview_active ? map.preview_progress : -1
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    Label {
                        text: qsTr("Slope angle overlay")
                        Layout.fillWidth: true
                    }
                    Switch {
                        // steepness terrain overlay, same mode as the fab button
                        checked: map.shared_config.overlay_mode === 101
                        onToggled: map.shared_config.overlay_mode = checked ? 101 : 0
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    Button {
                        text: _planner_model.optimisation_running ? qsTr("Optimizing…") : qsTr("Optimize Route")
                        enabled: _planner_model.point_count >= 2 && !_planner_model.optimisation_running
                        onClicked: _planner_model.optimise_route()
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Fly Route")
                        enabled: _planner_model.point_count >= 2
                        onClicked: {
                            _planner_model.fill_preview_route()
                            map.start_route_preview()
                        }
                        Layout.fillWidth: true
                    }
                    DirectionButton {
                        direction: gpx_root.weights_visible ? 270 : 90
                        onClicked: gpx_root.weights_visible = !gpx_root.weights_visible
                        ToolTip.visible: hovered
                        ToolTip.text: gpx_root.weights_visible ? qsTr("Hide optimisation weights")
                                                                : qsTr("Show optimisation weights")
                    }
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 0
                    to: 100
                    stepSize: 1
                    value: _planner_model.steepness_weight
                    onMoved: _planner_model.steepness_weight = value
                    formatCallback: function(value) { return qsTr("slope down ") + value.toFixed(0) + "%"; }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 0
                    to: 100
                    stepSize: 1
                    value: _planner_model.uphill_steepness_weight
                    onMoved: _planner_model.uphill_steepness_weight = value
                    formatCallback: function(value) { return qsTr("slope up ") + value.toFixed(0) + "%"; }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 0
                    to: 100
                    stepSize: 5
                    value: _planner_model.uphill_penalty
                    onMoved: _planner_model.uphill_penalty = value
                    formatCallback: function(value) { return qsTr("climb ") + (value / 10).toFixed(1); }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 0
                    to: 100
                    stepSize: 1
                    value: _planner_model.distance_weight
                    onMoved: _planner_model.distance_weight = value
                    formatCallback: function(value) { return qsTr("dist ") + value.toFixed(0) + "%"; }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 0
                    to: 100
                    stepSize: 1
                    value: _planner_model.cross_slope_weight
                    onMoved: _planner_model.cross_slope_weight = value
                    formatCallback: function(value) { return qsTr("cross ") + value.toFixed(0) + "%"; }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                LabledSlider {
                    visible: gpx_root.weights_visible
                    from: 5
                    to: 150
                    stepSize: 5
                    value: _planner_model.search_radius
                    onMoved: _planner_model.search_radius = value
                    formatCallback: function(value) { return qsTr("radius ") + value.toFixed(0) + "%"; }
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                Label {
                    visible: _planner_model.status !== ""
                    text: _planner_model.status
                    wrapMode: Label.Wrap
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                ListView {
                    id: point_list
                    // recorded tracks carry thousands of points; the editable
                    // point list is only shown for hand-planned routes
                    visible: !_planner_model.route_imported
                    model: visible ? _planner_model.points : 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? Math.min(contentHeight, 220) : 0
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    // flickables do not consume wheel events: without this
                    // the wheel would propagate to the map and zoom it
                    MouseArea {
                        parent: point_list
                        anchors.fill: point_list
                        acceptedButtons: Qt.NoButton // clicks pass through
                        onWheel: (wheel) => {
                            const max_y = Math.max(0, point_list.contentHeight - point_list.height);
                            point_list.contentY = Math.max(0, Math.min(max_y, point_list.contentY - wheel.angleDelta.y * 0.5));
                            wheel.accepted = true;
                        }
                    }
                    spacing: 2
                    clip: true

                    delegate: RowLayout {
                        width: point_list.width

                        Label {
                            text: (index + 1) + ": "
                                  + modelData.latitude.toFixed(5) + ", "
                                  + modelData.longitude.toFixed(5) + " ("
                                  + modelData.elevation.toFixed(0) + " m)"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        DirectionButton {
                            direction: 90
                            enabled: index > 0
                            onClicked: _planner_model.move_point_up(index)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Move point up")
                        }
                        DirectionButton {
                            direction: 270
                            enabled: index < _planner_model.point_count - 1
                            onClicked: _planner_model.move_point_down(index)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Move point down")
                        }
                        ToolButton {
                            text: "×"
                            onClicked: _planner_model.delete_point(index)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Delete point")
                        }
                    }
                }

                TextField {
                    id: name_input
                    placeholderText: qsTr("Name")
                    text: _planner_model.name
                    onTextEdited: _planner_model.name = text
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                TextArea {
                    id: description_input
                    placeholderText: qsTr("Description")
                    text: _planner_model.description
                    onTextEdited: _planner_model.description = text
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    wrapMode: TextArea.Wrap
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.bottomMargin: 10

                    Button {
                        text: qsTr("Save")
                        enabled: _planner_model.route_dirty
                        Layout.fillWidth: true
                        onClicked: _planner_model.save_route(_planner_model.name)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Save this route locally")
                    }
                    Button {
                        text: qsTr("Reset")
                        onClicked: _planner_model.reset()
                    }
                    Button {
                        text: qsTr("Export GPX")
                        enabled: _planner_model.point_count > 0
                        onClicked: {
                            // suggest the route name (or "route") as file name
                            const base = _planner_model.name.trim();
                            export_dialog.currentFile = export_dialog.currentFolder + "/" + (base !== "" ? base : "route") + ".gpx";
                            export_dialog.open()
                        }
                    }
                }
            }
        }

        // keep the editable fields in sync with an imported track
        Connections {
            target: _planner_model
            function onPointsChanged() {
                // the preview follows a snapshot of the route; stop it when
                // the route changes
                if (map.preview_active)
                    map.stop_route_preview();
            }
            // metadata comes from imports and resets; never clobber a field
            // the user is currently typing in
            function onNameChanged() {
                if (!name_input.activeFocus)
                    name_input.text = _planner_model.name;
            }
            function onDescriptionChanged() {
                if (!description_input.activeFocus)
                    description_input.text = _planner_model.description;
            }
        }

        // compact status bar shown while minimized: keeps the map visible and
        // clickable (edit mode stays active, new points update the stats live)
        RowLayout {
            id: minimized_bar
            visible: gpx_root.minimized
            anchors.fill: parent
            anchors.margins: 8

            Label {
                // keep the bar narrow: only the bigger of gain/loss is shown
                text: (_planner_model.distance / 1000).toFixed(1) + " km | "
                      + (_planner_model.ascent >= _planner_model.descent
                         ? "+ " + _planner_model.ascent.toFixed(0)
                         : "- " + _planner_model.descent.toFixed(0)) + " m"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                focusPolicy: Qt.NoFocus
                text: qsTr("Undo")
                enabled: _planner_model.point_count > 0
                onClicked: _planner_model.undo_last_point()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Delete the last route point")
            }
            ToolButton {
                text: _planner_model.optimisation_running ? qsTr("…") : qsTr("Optimize")
                enabled: _planner_model.point_count >= 2 && !_planner_model.optimisation_running
                onClicked: _planner_model.optimise_route()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Optimize the route")
            }
            DirectionButton {
                direction: 90
                onClicked: gpx_root.minimized = false
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Show route planner")
            }
            ToolButton {
                text: "×"
                onClicked: gpx_root.closeRequested()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close the route planner")
            }
        }
    }
}
