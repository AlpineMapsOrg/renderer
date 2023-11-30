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
import QtQuick.Layouts
import QtQuick.Controls.Material
import Alpine

RoundButton {
    property alias image: img.source
    property alias image_opacity: img.opacity
    property alias rotation: img.rotation
    property real size: 64
    property real image_size: 24
    property string tilted_caption: ""

    id: root
    implicitWidth: root.size
    implicitHeight: root.size
    radius: root.size / 4.0
    Material.elevation: 1;
    opacity: 0.9

    Image {
        id: img
        anchors.centerIn: parent
        width: image_size
        height: image_size
        opacity: 0.8
        smooth: true
    }

    Rectangle {
        anchors.fill: parent;
        anchors.margins: 5;
        color: "transparent"
        radius: parent.radius
        border { width:2; color:Qt.alpha( "black", 0.5); }
    }

    /*
    Label {
        visible: root.tilted_caption != ""
        text: root.tilted_caption
        rotation: -90
        font.bold: true
        font.pixelSize: 16
        //anchors.centerIn: parent
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 20
        //y: -parent.height
        //x: -this.width / 2.0
    }*/
}
