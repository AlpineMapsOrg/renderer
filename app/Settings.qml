/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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
import QtQuick.Layouts
import Alpine

Rectangle {
    id: settings_root
    color: "#00FFFFFF"
    property TerrainRenderer renderer
    Component.onCompleted: {
        // when creating the this component, values are read from the renderer
        // after that we establish a binding, so this component can set values on the renderer
        frame_rate_slider.value = renderer.frame_limit
        lod_slider.value = renderer.render_quality
        fov_slider.value = renderer.field_of_view
        renderer.frame_limit = Qt.binding(function() { return frame_rate_slider.value })
        renderer.render_quality = Qt.binding(function() { return lod_slider.value })
        renderer.field_of_view = Qt.binding(function() { return fov_slider.value })
    }

    Rectangle {
        color: "#88FFFFFF"
        height: layout.implicitHeight + 20
        anchors {
            left: settings_root.left
            right: settings_root.right
            bottom: settings_root.bottom
            margins: 10
        }

        ColumnLayout {
            id: layout
            anchors {
                fill: parent
                margins: 10
            }
            RowLayout {
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Quit")
                    onClicked: {
                        Qt.callLater(Qt.quit)
                    }
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Update")
                    onClicked: {
                        renderer.update()
                    }
                }
            }
            RowLayout {
                Label {
                    text: qsTr("Field of view:")
                }
                Slider {
                    Layout.fillWidth: true
                    id: fov_slider
                    from: 15
                    to: 120
                    stepSize: 1
                }
                Label {
                    text: fov_slider.value
                }
            }
            RowLayout {
                Label {
                    text: qsTr("Frame limiter:")
                }
                Slider {
                    Layout.fillWidth: true
                    id: frame_rate_slider
                    from: 2
                    to: 120
                    stepSize: 1
                }
                Label {
                    text: frame_rate_slider.value
                }
            }
            RowLayout {
                Label {
                    text: qsTr("Level of detail:")
                }
                Slider {
                    Layout.fillWidth: true
                    id: lod_slider
                    from: 0.1
                    to: 2.0
                    stepSize: 0.1
                }
                Label {
                    text: Number(lod_slider.value).toFixed(1)
                }
            }
        }
    }
}
