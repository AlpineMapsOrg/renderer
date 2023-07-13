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
import QtQuick.Controls.Material
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
        cache_size_slider.value = renderer.tile_cache_size
        renderer.frame_limit = Qt.binding(function() { return frame_rate_slider.value })
        renderer.render_quality = Qt.binding(function() { return lod_slider.value })
        renderer.field_of_view = Qt.binding(function() { return fov_slider.value })
        renderer.tile_cache_size = Qt.binding(function() { return cache_size_slider.value })
    }

    Rectangle {
        color: Qt.alpha(Material.backgroundColor, 0.7)
        height: settings_root.height / 2
        anchors {
            left: settings_root.left
            right: settings_root.right
            bottom: settings_root.bottom
        }

        TabBar {
            id: navigation_bar
            currentIndex: view.currentIndex
            anchors {
                left: settings_root.left
                right: settings_root.right
                top: parent.top
            }
            contentWidth: parent.width
            position: TabBar.Header
            TabButton {
                text: qsTr("View")
                width: implicitWidth + 20
            }
            TabButton {
                text: qsTr("Network and Cache")
                width: implicitWidth + 20
            }
        }

        SwipeView {
            id: view
            currentIndex: navigation_bar.currentIndex
            anchors {
                left: parent.left
                right: parent.right
                top: navigation_bar.bottom
                bottom: parent.bottom
            }
            Item {
                ColumnLayout {
                    anchors {
                        margins: 10
                        left: parent.left
                        right: parent.right
                        top: parent.top
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
            Item {
                ColumnLayout {
                    anchors {
                        margins: 10
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }

                    RowLayout {
                        Label {
                            text: qsTr("Cache size:")
                        }
                        Slider {
                            id: cache_size_slider
                            Layout.fillWidth: true
                            from: 1000
                            to: 20000
                            stepSize: 1000
                        }
                        Label {
                            text: cache_size_slider.value
                        }
                    }
                    RowLayout {
                        Label {
                            text: qsTr("Cached:")
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            id: cache_fill_slider
                            from: 0
                            to: cache_size_slider.value
                            value: renderer.cached_tiles
                        }
                        Label {
                            text: cache_fill_slider.value
                        }
                    }
                    RowLayout {
                        Label {
                            text: qsTr("Queued:")
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            id: queued_slider
                            from: 0
                            to: 200
                            value: renderer.queued_tiles
                        }
                        Label {
                            text: Number(queued_slider.value).toFixed(1)
                        }
                    }
                }
            }
        }
    }
}
