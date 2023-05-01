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
    property TerrainRenderer renderer

    GnssInformation {
        id: gnss
        enabled: current_location.checked
        onInformation_updated: {
            renderer.set_position(gnss.latitude, gnss.longitude)
        }
    }

    Image {
        id: camera_operation_centre
        source: "qrc:/icons/camera_operation_centre.svg"
        width: 16
        height: 16
        sourceSize: Qt.size(width, height)
        x: renderer.camera_operation_centre.x - width / 2
        y: renderer.camera_operation_centre.y - 60 - height / 2
        visible: renderer.camera_operation_centre_visibility && punkt.checked
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
            x: model.x * label_view.width / renderer.camera_width
            y: model.y * (label_view.height + 60) / renderer.camera_height - 60
            z:  50 * my_scale()
            Image {
                id: icon
                source: "qrc:/icons/peak.svg"
                width: 16 * my_scale()
                height: 16 * my_scale()
                sourceSize: Qt.size(width, height)
                x: -width/2
                y: -height
                anchors.centerIn: parent
            }

            Rectangle {
                id: text_rect
                x: -(width) / 2
                y: -icon.height - 20 * my_scale() - (height) / 2
                color: "#00FFFFFF"
                width: text.implicitWidth + 10
                height: text.implicitHeight + 5

                Glow {
                    anchors.fill: text
                    source: text
                    color: "#CCCCCC"
                    radius: 3
                    samples: 5
                    scale: text.scale
                }
//                Row {
//                    Text { font.pointSize: 24; text: "Normal" }
//                    Text { font.pointSize: 24; text: "Raised"; style: Text.Raised; styleColor: "#AAAAAA" }
//                    Text { font.pointSize: 24; text: "Outline";style: Text.Outline; styleColor: "red" }
//                    Text { font.pointSize: 24; text: "Sunken"; style: Text.Sunken; styleColor: "#AAAAAA" }
//                }
                Text {
                    anchors.fill: parent
                    id: text
                    color: "#000000"
                    text: model.text + "(" + model.altitude + "m)"
                    font.pixelSize: 20 * my_scale()
                    scale: 20 * my_scale() / font.pixelSize
                }
//                Text {
//                    anchors {
//                        horizontalCenter: text_rect
//                        bottom: text.top
//                    }
//                    id: text2
//                    color: "#000000"
//                    text: model.text + "(" + model.altitude + "m)"
//                    font.pixelSize: 20 * my_scale()
//                    style: Text.Outline; styleColor: "#CCCCCC"
////                    scale: 20 * my_scale() / font.pixelSize
//                }
            }

        }
    }

    RoundButton {
        id: punkt
        width: 60
        height: 60
        checkable: true
        checked: true
        focusPolicy: Qt.NoFocus
        text: "punkt"
        visible: false
        anchors {
            right: parent.right
            bottom: compass.top
            rightMargin: 10
            bottomMargin: 10
        }
    }
    RoundButton {
        id: compass
        width: 60
        height: 60
        rotation: renderer.camera_rotation_from_north
        onClicked: renderer.rotate_north()
        focusPolicy: Qt.NoFocus
        icon {
            source: "qrc:/icons/compass.svg"
            height: 40
            width: 40
        }
        anchors {
            right: parent.right
            bottom: current_location.top
            rightMargin: 10
            bottomMargin: 10
        }
    }

    RoundButton {
        id: current_location
        width: 60
        height: 60
        checkable: true
        focusPolicy: Qt.NoFocus
        icon {
            source: "qrc:/icons/current_location.svg"
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
