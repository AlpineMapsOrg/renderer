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

    property var vector: Qt.vector4d(0.0, 0.0, 0.0, 0.0);
    property int vectorSize: -1;
    property alias dialogTitle: editDialog.title;
    property var elementNames: ["X", "Y", "Z", "W"];
    property var elementFroms: [0.0, 0.0, 0.0, 0.0];
    property var elementTos: [1.0, 1.0, 1.0, 1.0];

    function evaluate_v_size() {
        var tmp = 0;
        if ('x' in vector && typeof vector.x === 'number') tmp++;
        if ('y' in vector && typeof vector.y === 'number') tmp++;
        if ('z' in vector && typeof vector.z === 'number') tmp++;
        if ('w' in vector && typeof vector.w === 'number') tmp++;
        vectorSize = tmp;
    }

    function vector_to_string() {
        var tmp = "";
        tmp += vector.x.toFixed(2).toString();
        if (vectorSize > 1) tmp += " | " + vector.y.toFixed(2).toString();
        if (vectorSize > 2) tmp += " | " + vector.z.toFixed(2).toString();
        if (vectorSize > 3) tmp += " | " + vector.w.toFixed(2).toString();
        return tmp;
    }

    onVectorChanged: {
        if (vectorSize < 0) evaluate_v_size();
        label.text = vector_to_string();
    }

    // Define the Dialog
    Dialog {
        id: editDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 300
        background: Rectangle {
            color: Material.backgroundColor
            radius: 0
        }

        visible: false

        // Content of the Dialog
        GridLayout {
            anchors.fill: parent
            columns: 2
            Text { text: elementNames[0] + ": " }
            ValSlider {
                id: edit1
                from: elementFroms[0]; to: elementTos[0];
                onMoved: {
                    root.vector.x = value;
                    root.vector = root.vector; // Otherwise onVectorChanged not triggered...
                }
            }

            Text { text: elementNames[1] + ": "; visible: vectorSize > 1}
            ValSlider {
                id: edit2
                from: elementFroms[1]; to: elementTos[1];
                visible: vectorSize > 1
                onMoved: {
                    root.vector.y = value;
                    root.vector = root.vector; // Otherwise onVectorChanged not triggered...
                }
            }

            Text { text: elementNames[2] + ": "; visible: vectorSize > 2 }
            ValSlider {
                id: edit3
                from: elementFroms[2]; to: elementTos[2];
                visible: vectorSize > 2
                onMoved: {
                    root.vector.z = value;
                    root.vector = root.vector; // Otherwise onVectorChanged not triggered...
                }
            }

            Text { text: elementNames[3] + ": "; visible: vectorSize > 3 }
            ValSlider {
                id: edit4
                from: elementFroms[3]; to: elementTos[3];
                visible: vectorSize > 3
                onMoved: {
                    root.vector.w = value;
                    root.vector = root.vector; // Otherwise onVectorChanged not triggered...
                }
            }

        }
    }

    Label {
        id: label;
        padding: 5;
        text: "#FFFFFFF";
        font.underline: true;
        Layout.fillWidth: true;

        MouseArea{
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                edit1.value = root.vector.x
                if (vectorSize > 1) edit2.value = root.vector.y
                if (vectorSize > 2) edit3.value = root.vector.z
                if (vectorSize > 3) edit4.value = root.vector.w
                editDialog.open()
            }
        }
    }

    height: label.height;
    Layout.fillWidth: true;

}
