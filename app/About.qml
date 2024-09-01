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
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: about_root
    color: Qt.alpha(Material.backgroundColor, 0.7)
    Flickable {
        anchors {
            fill: parent
            margins: 10
        }

        contentHeight: about_content.height
        contentWidth: about_content.width
        flickableDirection: Flickable.AutoFlickIfNeeded

        Rectangle {
            id: about_content
            width: logo.width + logo_type.width + 20
            color: "#00FFFFFF"
            height: about_text.implicitHeight + logo.height + 20
            Image { id: logo; width: 120; height: 120; source: _r + "icons/mascot.jpg" }
            Image {
                id: logo_type
                anchors {
                    left: logo.right
                    top: logo.top
                    leftMargin: 10
                    topMargin: 0
                    bottomMargin: 0
                }
                fillMode: Image.PreserveAspectFit
                width: 180
                height: 120
                source: _r + "icons/logo_type_vertical.png"
            }


            Text {
                id: about_text
                anchors {
                    left: logo.left
                    top: logo.bottom
                    topMargin: 10
                }
                width: about_root.width - 20
                wrapMode: Text.Wrap
                onLinkActivated: Qt.openUrlExternally(link)
                textFormat: Text.MarkdownText
                text: qsTr("
This is an open source application. It is **released** under the GNU General Public License (version 3 or any later version). The source code is available on [github.com/AlpineMapsOrg/renderer](https://github.com/AlpineMapsOrg/renderer).

The source of elevation and orthographic photo data is [basemap.at](https://basemap.at),
it is licensed under the Open Government Data Austria license (CC-BY 4.0).

The source of POI feature labels is [openstreetmap.org](https://www.openstreetmap.org/copyright),
it is licensed under the Open Data Commons Open Database License (ODbL) by the OpenStreetMap Foundation (OSMF).

## Authors:
Adam Celarek, Lucas Dworschak, Gerald Kimmersdorfer, Jakob Lindner, Patrick Komon, Jakob Maier

## Impressum:
Adam Celarek\\
Hartmanngasse 12/22\\
1050 Wien\\
Österreich / Austria")
            }
        }
    }
}
