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
import Alpine

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
            width: logo.width + Math.max(alpine_text.width, maps_text.width) + 20
            color: "#00FFFFFF"
            height: about_text.implicitHeight + logo.height + 20
            Image { id: logo; width: 120; height: 120; source: "icons/mascot.jpg" }
            Text {
                id: alpine_text
                anchors {
                    left: logo.right
                    top: logo.top
                    leftMargin: 10
                    topMargin: 0
                    bottomMargin: 0
                }
                text: "Alpine"
                color: Material.primaryTextColor
                font {
                    weight: 800
                    pixelSize: 50
                }
            }
            Text {
                id: maps_text
                anchors {
                    left: logo.right
                    top: alpine_text.baseline
                    leftMargin: 10
                    topMargin: 10
                    bottomMargin: 0
                }
                text: "Maps"
                color: Material.primaryTextColor
                font {
                    weight: 400
                    pixelSize: 50
                }
            }
            Text {
                id: dotorg_text
                anchors {
                    left: maps_text.right
                    baseline: maps_text.baseline
                    leftMargin: 0
                }
                text: ".org"
                color: Material.primaryTextColor
                font {
                    weight: 100
                    pixelSize: 30
                }
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

                text: qsTr("<p>AlpineMaps.org is an open source application. It is released under the GNU General Public License (version 3 or any later version). "
                           + "The source code is available on <a href=\"https://github.com/AlpineMapsOrg/renderer\">github.com/AlpineMapsOrg/renderer</a>.</p>"
                           + "<p>The source of elevation and orthographic photo data is <a href=\"https://basemap.at\">basemap.at</a>, "
                           + "it is licensed under the Open Government Data Austria license (CC-BY 4.0).</p>"
                           + "<h3>Authors:</h3><p>Adam Celarek, Lucas Dworschak, Gerald Kimmersdorfer, Jakob Lindner<p>")
            }
        }
    }
}
