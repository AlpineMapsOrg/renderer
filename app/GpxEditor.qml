/*****************************************************************************
 * AlpineMaps.org
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
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

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

    function format_distance(d) {
        return d < 1000 ? d.toFixed(0) + " m" : (d / 1000).toFixed(2) + " km";
    }

    // start minimized on narrow screens (phone portrait), where the expanded
    // panel would cover the whole map
    Component.onCompleted: minimized = gpx_root.width < 500

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

        ColumnLayout {
            id: content_column
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8
            visible: !gpx_root.minimized

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Route Planner")
                    font.bold: true
                    font.pointSize: 16
                    Layout.fillWidth: true
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
                text: qsTr("Click on the map to add route points. Dragging still moves the camera.")
                wrapMode: Label.Wrap
                Layout.fillWidth: true
            }

            TextField {
                id: name_input
                placeholderText: qsTr("Name")
                text: _planner_model.name
                onTextEdited: _planner_model.name = text
                Layout.fillWidth: true
            }

            TextArea {
                id: description_input
                placeholderText: qsTr("Description")
                text: _planner_model.description
                onTextEdited: _planner_model.description = text
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                wrapMode: TextArea.Wrap
            }

            Label {
                text: _planner_model.points.length + " points | "
                      + format_distance(_planner_model.distance) + " | ↑ "
                      + _planner_model.ascent.toFixed(0) + " m | ↓ "
                      + _planner_model.descent.toFixed(0) + " m"
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Steepness: avg ")
                      + _planner_model.avg_steepness.toFixed(1) + "° / max "
                      + _planner_model.max_steepness.toFixed(1) + "°"
                Layout.fillWidth: true
            }

            ListView {
                id: point_list
                model: _planner_model.points
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
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

            RowLayout {
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

            // keep the editable fields in sync with an imported track
            Connections {
                target: _planner_model
                function onPointsChanged() {
                    if (!name_input.activeFocus)
                        name_input.text = _planner_model.name;
                    if (!description_input.activeFocus)
                        description_input.text = _planner_model.description;
                }
                // fly to the start of an imported track, like the upload button does
                function onTrack_imported(position) {
                    if (position.x !== 0 && position.y !== 0)
                        map.set_position(position.x, position.y)
                }
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
                text: _planner_model.points.length + " pts | "
                      + format_distance(_planner_model.distance)
                elide: Text.ElideRight
                Layout.fillWidth: true
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
