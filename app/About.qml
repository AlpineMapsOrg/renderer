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
    id: about_root
    color: "#88FFFFFF"
    Flickable {
        anchors {
            fill: parent
            margins: 10
        }

        contentHeight: about_text.implicitHeight
        contentWidth: about_text.width
        flickableDirection: Flickable.AutoFlickIfNeeded
        Text {
            id: about_text
            width: about_root.width - 20
            wrapMode: Text.Wrap
            onLinkActivated: Qt.openUrlExternally(link)

            text: qsTr("<h2>AlpineMaps.org</h2>"
                       + "<p><img src=\"qrc:/alpinemaps/app/icons/mascot.png\" width=\"200\" height=\"212\" /><br /></ p>"
                       + "<p>AlpineMaps.org is an open source application. It is released under the GNU General Public License (version 3 or any later version)."
                       + "The source code is available on <a href=\"https://github.com/AlpineMapsOrg/renderer\">github.com/AlpineMapsOrg/renderer</a>.<p>"
                       + "<p>The source of elevation and orthographic photo data is <a href=\"https://basemap.at\">basemap.at</a>, "
                       + "it is licensed under the Open Government Data Austria license (CC-BY 4.0).</p>"
                       + "<h3>Authors:</h3><p>Adam Celarek<p>")
        }
    }
}
