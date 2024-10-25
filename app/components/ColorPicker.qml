/*****************************************************************************
 * AlpineMaps.org
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
import QtQuick.Controls.Material
import QtQuick.Layouts
import "FluxColor" as Flux

Item {
    id: root;

    property color selectedColour;
    property bool alphaEnabled: true;

    RowLayout {
        anchors.fill: parent
        spacing: 5
        Rectangle {
            Layout.preferredHeight: parent.height - 10;
            Layout.preferredWidth: parent.height - 10;
            color: root.selectedColour;
            border { width:1; color:Qt.alpha( "white", 1.0); }
        }
        Label {
            id: label;
            padding: 5;
            Layout.fillWidth: !alphaEnabled;
            text: selectedColour.toString().toUpperCase()
            font.underline: true;

            MouseArea{
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    colorDialog.open();
                }
            }
        }
        Slider {
            id: slider;
            from: 0.0; to: 1.0; stepSize: 0.01;
            value: root.selectedColour.a
            Layout.fillWidth: true;
            visible: alphaEnabled;
            implicitHeight: label.implicitHeight;
        }
    }

    Popup {
        id: colorDialog
        anchors.centerIn: parent
        Flux.ColorChooser {
            anchors.fill: parent
            id: colour_chooser
            selected_colour: root.selectedColour
        }
        onClosed: {
            root.selectedColour = colour_chooser.new_colour
        }
    }
    height: label.height;
    Layout.fillWidth: true;

}
