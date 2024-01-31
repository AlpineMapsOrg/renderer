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

Item {
    id: root
    property alias iconSource: icon.source
    property alias text: label.text
    property alias hotkey: hotkey.text
    property int innerPadding: 14
    property bool selectable: true
    property int bid: {
        return Math.floor(Math.random() * 100000);
    }

    property variant drawer: parent.parentDrawer

    signal clicked()

    Layout.fillWidth: true
    height: rowlayout.implicitHeight + 2 * innerPadding
    Component.onCompleted: evaluateBackgroundColor(false)

    function evaluateBackgroundColor(hover) {
        if (hover) {
            background.color = Qt.alpha(Material.accentColor, 0.3)
        } else {
            if (drawer.selectedButtonId === bid) {
                background.color = Qt.alpha(Material.backgroundDimColor, 0.2)
            } else {
                background.color = "transparent"
            }
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: "transparent"
    }

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.drawer.handleClick(root);
        onEntered: root.evaluateBackgroundColor(true);
        onExited: root.evaluateBackgroundColor(false);
    }

    RowLayout {
        id: rowlayout
        spacing: innerPadding
        anchors.margins: innerPadding
        anchors.fill: parent

        Image {
            id: icon
            smooth: true
            opacity: 0.54
            fillMode: Image.Pad
            sourceSize: Qt.size (24, 24)
            verticalAlignment: Image.AlignVCenter
            horizontalAlignment: Image.AlignHCenter
        }

        Item {
            width: 36 - (2 * parent.spacing)
        }

        Label {
            id: label
            opacity: 0.87
            font.pixelSize: 14
            Layout.fillWidth: true
            font.weight: Font.Medium
        }

        Rectangle {
            width: hotkey.width+15;
            height: hotkey.height+5;
            opacity: 0.30
            border.color: "black"  // Set border color
            border.width: 2        // Set border width
            radius: 3
            color: "transparent"
            visible: root.hotkey !== ""
            Label {
                id: hotkey
                font.pixelSize: 14
                anchors.centerIn: parent
                font.weight: Font.Medium

            }
        }


    }
}
