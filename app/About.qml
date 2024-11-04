/*****************************************************************************
 * AlpineMaps.org
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
    color: Qt.alpha(Material.backgroundColor, 0.9)
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
            Image { id: logo; width: 120; height: 120; source: _r + "icons/icon.png" }
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


            Label {
                id: about_text
                anchors {
                    left: logo.left
                    top: logo.bottom
                    topMargin: 10
                }
                width: about_root.width - 20
                wrapMode: Text.Wrap
                onLinkActivated: Qt.openUrlExternally(link)
                textFormat: Text.StyledText
                linkColor: Material.accentColor
                text: qsTr("
This is an open source application. It is released under the GNU General Public License (version 3 or any later version).
The source code is available on <a href=\"https://github.com/AlpineMapsOrg/renderer\">github.com/AlpineMapsOrg/renderer</a>.
<br><br>
The source of elevation and orthographic photo data is <a href=\"https://basemap.at/\">basemap.at</a>,
it is licensed under the Open Government Data Austria license (CC-BY 4.0).
<br><br>
The source of POI feature labels is <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a>,
it is licensed under the Open Data Commons Open Database License (ODbL) by the OpenStreetMap Foundation (OSMF).
<br>

<h2>Authors:</h2>
Adam Celarek, Lucas Dworschak, Gerald Kimmersdorfer, Jakob Lindner, Patrick Komon, Jakob Maier
<br>

<h2>Impressum:</h2>
Adam Celarek<br>
Frankenberggasse 8/10<br>
1040 Wien<br>
Österreich / Austria<br>
<br>
E-Mail: alpinemaps.org@xibo.at
<br>")
            }
        }
    }
}
