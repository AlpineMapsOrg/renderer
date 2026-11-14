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
import QtCore

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

    // optimisation weight sliders, collapsed behind the chevron by default
    property bool weights_visible: false

    function format_distance(d) {
        return d < 1000 ? d.toFixed(0) + " m" : (d / 1000).toFixed(2) + " km";
    }

    // start minimized on narrow screens (phone portrait), where the expanded
    // panel would cover the whole map
    Component.onCompleted: minimized = gpx_root.width < 500

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
                    map.change_edit_mode(true);
                }
            }
        }
    }

    FileDialog {
        id: export_dialog
        title: qsTr("Export GPX track")
        nameFilters: ["GPX (*.gpx)"]
        defaultSuffix: "gpx"
        fileMode: FileDialog.SaveFile
        currentFolder: StandardPaths.standardLocations(StandardPaths.DownloadLocation)[0]
        onAccepted: _planner_model.export_gpx(export_dialog.selectedFile)
    }

    FileDialog {
        id: import_dialog
        title: qsTr("Import GPX track")
        nameFilters: ["GPX (*.gpx *.xml)"]
        fileMode: FileDialog.OpenFile
        onAccepted: _planner_model.import_gpx(import_dialog.selectedFile)
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

        // scrollable body: on small screens the content is taller than the
        // panel, and the buttons at the bottom must stay reachable
        Flickable {
            id: panel_scroll
            visible: !gpx_root.minimized
            anchors.fill: parent
            contentHeight: content_column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

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
                    ToolButton {
                        text: minimized ? "▲" : "▼"
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
                    text: _planner_model.points.length + " points | "
                          + format_distance(_planner_model.distance) + " | ↑ "
                          + _planner_model.ascent.toFixed(0) + " m | ↓ "
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
                        enabled: _planner_model.points.length >= 2 && !_planner_model.optimisation_running
                        onClicked: _planner_model.optimise_route()
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Fly Route")
                        enabled: _planner_model.points.length >= 2
                        onClicked: {
                            _planner_model.fill_preview_route()
                            map.start_route_preview()
                        }
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: gpx_root.weights_visible ? "▼" : "▲"
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
                    formatCallback: function(value) { return qsTr("slope ") + value.toFixed(0) + "%"; }
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
                    model: _planner_model.points
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 220)
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
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
                        ToolButton {
                            text: "▲"
                            enabled: index > 0
                            onClicked: _planner_model.move_point_up(index)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Move point up")
                        }
                        ToolButton {
                            text: "▼"
                            enabled: index < _planner_model.points.length - 1
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
                        text: qsTr("Import GPX")
                        onClicked: import_dialog.open()
                    }
                    Button {
                        text: qsTr("Reset")
                        onClicked: _planner_model.reset()
                    }
                    Button {
                        text: qsTr("Export GPX")
                        enabled: _planner_model.points.length > 0
                        onClicked: {
                            // suggest the route name (or "route") as file name
                            const base = _planner_model.name.trim();
                            export_dialog.currentFile = export_dialog.currentFolder + "/" + (base !== "" ? base : "route") + ".gpx";
                            export_dialog.open()
                        }
                    }
                    Item { Layout.fillWidth: true }
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
            // fly to the start of an imported track, like the upload button does
            function onTrack_imported(position) {
                if (position.x !== 0 && position.y !== 0)
                    map.set_position(position.x, position.y)
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
                         ? "↑ " + _planner_model.ascent.toFixed(0)
                         : "↓ " + _planner_model.descent.toFixed(0)) + " m"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                focusPolicy: Qt.NoFocus
                text: qsTr("Undo")
                enabled: _planner_model.points.length > 0
                onClicked: _planner_model.undo_last_point()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Delete the last route point")
            }
            ToolButton {
                text: _planner_model.optimisation_running ? qsTr("…") : qsTr("Optimize")
                enabled: _planner_model.points.length >= 2 && !_planner_model.optimisation_running
                onClicked: _planner_model.optimise_route()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Optimize the route")
            }
            ToolButton {
                text: "×"
                onClicked: gpx_root.closeRequested()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close the route planner")
            }
            ToolButton {
                text: "▲"
                onClicked: gpx_root.minimized = false
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Show route planner")
            }
        }
    }
}
