/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celerek
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

Rectangle {
    property alias checkable: button.checkable
    property alias checked: button.checked
    property alias rotation: button.rotation
    signal clicked()
    property string icon_source: ""
    width: 50
    height: 50
    radius: width
    color: button.checked ? Material.accentColor : Material.backgroundColor
    RoundButton {
        id: button
        width: parent.width + 10
        height: parent.height + 10
        highlighted: checked
        focusPolicy: Qt.NoFocus
        icon {
            source: icon_source
            height: 38
            width: 38
        }
        onClicked: parent.clicked()
        anchors.centerIn: parent
    }
}
