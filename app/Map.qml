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
import MyRenderLibrary

Rectangle {
    id: map_gui
    color: "#00000000"
    property MeshRenderer renderer

    RoundButton {
        id: current_location
        width: 60
        height: 60
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
        onClicked: {
            if (!renderer)
                return;
//            renderer.virtual_resolution_factor = 0.1
        }
    }
}
