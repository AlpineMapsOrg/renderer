/*****************************************************************************
 * Alpine Terrain Renderer
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
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    default property alias content: main_content.children
    property variant unhandledContent: ({});

    width: 270
    height: main_content.height
    x: 10
    y: parent.height / 2 - height / 2
    color:  Qt.alpha(Material.backgroundColor, 0.7)

    SetPanel {
        id: main_content
        maxHeight: map.height // needs to be set directly because parents height is dependent

        anchors {
            left: parent.left
            right: parent.right
        }

    }
}
