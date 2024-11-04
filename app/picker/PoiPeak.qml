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
        text: feature_title + " (" + feature_properties.ele + "m)";
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
                text: "Prominence:"
                font.bold: true
            }
            Label {
                text: feature_properties.prominence
            }
            Label {
                text: "Cross:"
                font.bold: true
            }
            Label {
                text: feature_properties.summit_cross
            }
            Label {
                text: "Links:"
                font.bold: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }
            Column {
                id: links
                Layout.fillWidth: true
                Button {
                    text: "Wikipedia"
                    visible: typeof feature_properties.wikipedia !== "undefined"
                    width: parent.width
                    onClicked: {
                        let link_split = feature_properties.wikipedia.split(":")
                        Qt.openUrlExternally("https://" + link_split[0] + ".wikipedia.org/wiki/" + link_split[1]);
                    }
                }
                Button {
                    text: "Wikidata"
                    visible: typeof feature_properties.wikidata !== "undefined"
                    width: parent.width
                    onClicked: {
                        Qt.openUrlExternally("https://www.wikidata.org/wiki/" + feature_properties.wikidata);
                    }
                }
            }
        }
    }

}
