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
import app


Rectangle {
    id: root
    color: "#00FFFFFF"

    GnssInformation {
        id: gnss
        enabled: true
        onInformation_updated: {

        }
    }

    Rectangle {
        anchors {
            centerIn: root
        }
        width: 200
        height: 200
        color: Qt.alpha(Material.backgroundColor, 0.7)

        ColumnLayout {
            anchors.fill: parent
            Label { text: `DD ${Number(gnss.latitude).toFixed(5)}, ${Number(gnss.longitude).toFixed(5)}` }
            Label { text: `Accuracy: ${Number(gnss.horizontal_accuracy).toFixed(0)}m` }
            Label { text: `Updated: `
                + `${String(gnss.timestamp.getHours()).padStart(2, '0')}:`
                + `${String(gnss.timestamp.getMinutes()).padStart(2, '0')}:`
                + `${String(gnss.timestamp.getSeconds()).padStart(2, '0')}` }
        }
    }
}
