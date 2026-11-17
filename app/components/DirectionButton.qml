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
import QtQuick.Controls.Material

// directional ToolButton drawn from the chevron icon png instead of a
// unicode glyph: arrows like ▲▼‹› are not in the font shipped to the
// webassembly build and render as empty boxes there
ToolButton {
    // qml rotation is clockwise: 0 = left, 90 = up, 180 = right, 270 = down
    property int direction: 0

    implicitWidth: 36
    implicitHeight: 36

    Image {
        anchors.centerIn: parent
        source: _r + "icons/material/chevron_left.png"
        rotation: parent.direction
        width: 18
        height: 18
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
