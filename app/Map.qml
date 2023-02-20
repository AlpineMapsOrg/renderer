/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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
import Qt5Compat.GraphicalEffects

Rectangle {
    id: map_gui
    color: "#00000000"
    property MeshRenderer renderer

    GnssInformation {
        id: gnss
        enabled: current_location.checked
        onInformation_updated: {
            renderer.set_position(gnss.latitude, gnss.longitude)
        }
    }

    Repeater {
        id: label_view
        anchors.fill: parent

        model: CameraTransformationProxyModel {
            sourceModel: LabelModel {}
            camera: renderer.camera
        }

        delegate: Rectangle {
            function my_scale() {

                let distance_scale = Math.max(0.4, model.size)
                let importance_scale = model.importance / 10;
                let min_scale = 0.1;
                return (min_scale + (1.0-min_scale) * importance_scale) * distance_scale * 2.0;
            }
            x: model.x * label_view.width / renderer.frame_buffer_width
            y: model.y * (label_view.height + 60) / renderer.frame_buffer_height - 60
            Image {
                id: icon
                source: "qrc:/alpinemaps/app/icons/peak.svg"
                width: 16 * my_scale()
                height: 16 * my_scale()
                sourceSize: Qt.size(width, height)
                x: -width/2
                y: -height
                anchors.centerIn: parent
            }

            Rectangle {
                x: -(text.implicitWidth + 10) / 2
                y: -icon.height - 20 * my_scale() - (text.implicitHeight + 5) / 2
                color: "#00FFFFFF"
                width: text.implicitWidth + 10
                height: text.implicitHeight + 5

                Glow {
                    anchors.fill: text
                    source: text
                    color: "white"
                    radius: 3
                    samples: 5
                }
                Label {
                    anchors.centerIn: parent
                    id: text
                    color: "#000000"
                    text: model.text
                    font.pointSize: 20 * my_scale()
                }
            }

        }
    }

    RoundButton {
        id: current_location
        width: 60
        height: 60
        checkable: true
        icon {
            source: "qrc:/alpinemaps/app/icons/current_location.svg"
            height: 32
            width: 32
        }
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
    }
}
