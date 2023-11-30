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
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root;
    property alias from: slider.from;
    property alias to: slider.to;
    property alias stepSize: slider.stepSize;
    property alias first: slider.first;
    property alias second: slider.second;
    property alias snapMode: slider.snapMode;
    property var formatCallback: defaultFormatCallback;
    signal moved();

    function defaultFormatCallback(value) {
        return Math.round(slider.first.value) + " - " + Math.round(slider.second.value);
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        RangeSlider {
            id: slider;
            Layout.preferredWidth: root.width - 50
        }
        Label {
            id: label;
            Layout.preferredWidth: 50
            text: {
                return root.formatCallback(root.value);
            }
            font.underline: true;
        }
    }
    height: slider.implicitHeight;
    Layout.fillWidth: true;
}
