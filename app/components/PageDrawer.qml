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

Drawer {
    default property alias content: item_layout.children

    id: drawer
    implicitHeight: map.height
    width: Math.min (parent.width > parent.height ? 400 : 300,
                             Math.min (parent.width, parent.height) * 0.90)
    Material.elevation: 2;

    property string bannerTitle: ""
    property string bannerSubtitle: ""
    property string bannerIconSource: ""
    property size bannerIconSize: Qt.size (72, 72)

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

            RowLayout {
                spacing: 16

                anchors {
                    fill: parent
                    centerIn: parent
                    margins: 16
                }

                Image {
                    source: bannerIconSource
                    sourceSize: bannerIconSize
                }

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Item {
                        Layout.fillHeight: true
                    }

                    Label {
                        color: "#1b1b1b"
                        text: bannerTitle
                        font.weight: Font.Medium
                        font.pixelSize: 24
                    }

                    Label {
                        color: "#1b1b1b"
                        opacity: 0.87
                        text: bannerSubtitle
                        font.pixelSize: 14
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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
