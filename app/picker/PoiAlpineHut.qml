/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Lucas Dworschak
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
import app

Rectangle {
    id: root
    Label {
        id: title
        padding: 10
        anchors {
            top: root.top
            left: root.left
            right: root.right
        }
        text: feature_title;
        font.pixelSize: 18
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
    Flickable {
        anchors {
            margins: 10
            top: title.bottom
            left: root.left
            right: root.right
        }
        GridLayout {
            anchors.fill: parent
            columns: 2
            Label {
                text: "Coordinates: "
                font.bold: true
            }
            TextEdit {
                text: feature_properties.latitude.toFixed(5) + " / " + feature_properties.longitude.toFixed(5)
            }
            Label {
                text: "Altitude:"
                font.bold: true
            }
            Label {
                text: feature_properties.altitude.toFixed(0)
            }
            Label {
                text: ""
                font.bold: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            Column {
                id: links
                Layout.fillWidth: true
                Button {
                    text: "Call: " + feature_properties.phone
                    visible: typeof feature_properties.phone !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally("tel:" + feature_properties.phone);
                    }
                }
                Button {
                    text: "Web: " + feature_properties.website
                    visible: typeof feature_properties.website !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally(feature_properties.website);
                    }
                }
            }
        }
    }

}
