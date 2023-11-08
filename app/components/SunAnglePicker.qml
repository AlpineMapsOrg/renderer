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
    id: root
    property vector2d sun_angles: vector2d(0.0, 0.0);
    property bool enabled: true;
    //property alias sun_phi: phi.value
    //property alias sun_theta: theta.value

    Layout.fillHeight: true;
    Layout.preferredHeight: 75;
    Layout.fillWidth: true;

    RowLayout {
        height: 75;
        Dial {
            id: phi
            Layout.fillHeight: true;
            //implicitWidth: 70
            wrap: true
            onValueChanged: {
                root.sun_angles = Qt.vector2d(value, sun_angles.y);
                phi_label.text = value.toFixed(2) + "°";
            }
            value: sun_angles.x
            from: 0
            enabled: root.enabled
            opacity: root.enabled ? 1.0 : 0.5
            to: 360
            Label {
                id: phi_label
                x: 75 / 2 - this.width / 2
                y: 75 / 2 - this.height / 2
            }
        }
        Slider {
            id: theta
            from: -180
            to: 180
            Layout.fillHeight: true;
            orientation: Qt.Vertical
            value: sun_angles.y
            onValueChanged: {
                root.sun_angles = Qt.vector2d(sun_angles.x, value);
                theta_label.text = value.toFixed(2) + "°";
            }
            stepSize: 1
            enabled: root.enabled
            opacity: root.enabled ? 1.0 : 0.5
            Label {
                id: theta_label
                x: 40
                y: parent.height / 2 - this.height / 2
            }

        }

    }


}
