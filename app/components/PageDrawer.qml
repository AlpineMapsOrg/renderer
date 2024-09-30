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


Drawer {
    default property alias content: item_layout.children

    id: drawer
    implicitHeight: map.height
    width: Math.min (400, Math.min (parent.width, parent.height) * 0.80)
    Material.elevation: 2;

    property color iconBgColorLeft: Material.backgroundColor
    property color iconBgColorRight: Qt.alpha(Material.accentColor, 0.2)

    property int selectedButtonId: -1
    function handleClick(button) {
        if (button.selectable) {
            if (selectedButtonId !== button.bid) {
                selectedButtonId = button.bid
                button.clicked();

                for (var i = 0; i < content.length; i++) {
                    if (typeof content[i].evaluateBackgroundColor !== "undefined")
                        content[i].evaluateBackgroundColor(false);
                }
            }
        } else {
            button.clicked();
        }
        drawer.close();
    }

    background: Rectangle {
        anchors.fill: parent
        color:  Qt.alpha(Material.backgroundColor, 0.8)
    }
    topPadding: 0
    bottomPadding: 0

    ColumnLayout {
        spacing: 0
        anchors.margins: 0
        anchors.fill: parent

        //
        // BANNER
        //
        Rectangle {
            z: 1
            height: 120
            id: banner
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: iconBgColorLeft  }
                    GradientStop { position: 1.0; color: iconBgColorRight }
                }
            }
            Image {
                id: logo_mark
                source: _r + "icons/icon.png"
                sourceSize: Qt.size (64, 64)
                fillMode: Image.PreserveAspectFit
                width: banner.width * 0.16
                anchors {
                    left: banner.left
                    verticalCenter: banner.verticalCenter
                    margins: 16
                }
            }
            Image {
                id: logo_type
                source: _r + "icons/logo_type_horizontal_short.png"
                width: banner.width * 0.65
                fillMode: Image.PreserveAspectFit
                anchors {
                    left: logo_mark.right
                    verticalCenter: banner.verticalCenter
                    leftMargin: banner.width * 0.02
                }
            }
            Label {
                color: "#1b1b1b"
                opacity: 0.87
                text: _alpine_renderer_version
                font.pixelSize: 10
                anchors {
                    right: banner.right
                    bottom: banner.bottom
                    margins: 8
                }
            }
        }

        //
        // ITEMS
        //
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                anchors.fill: parent
            }

            ColumnLayout {
                property alias parentDrawer: drawer
                id: item_layout
                spacing: 0
                anchors.margins: 0
                anchors.fill: parent
            }
        }
    }
}
