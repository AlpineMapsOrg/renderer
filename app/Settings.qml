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
import MyRenderLibrary

Rectangle {
    id: settings_root
    width: parent.width
    height: parent.height
    color: "#88FFFFFF"

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 10
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
                text: qsTr("Frame limiter:")
            }
            Slider {
                Layout.fillWidth: true
                id: frame_rate_slider
                from: 1
                to: 120
                stepSize: 1
                value: 60
            }
            Label {
                text: frame_rate_slider.value
            }
        }
        RowLayout {
            Label {
                text: qsTr("Virtual Resolution factor:")
            }
            Slider {
                Layout.fillWidth: true
                id: virtual_resolution_factor
                from: 0.1
                to: 1.0
                stepSize: 0.1
                value: 0.5
            }
            Label {
                text: Number(virtual_resolution_factor.value).toFixed(1)
            }
        }
    }
}
