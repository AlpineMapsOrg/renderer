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

    property date selectedDate: new Date(0);
    signal editingFinished();

    onSelectedDateChanged: {
        label.text = Qt.formatDate(selectedDate, "dd.MM.yyyy");
    }

    // Define the Dialog
    Dialog {
        id: editDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        height: 400
        background: Rectangle {
            color: Material.backgroundColor
            radius: 0
        }

        visible: false

        DateMonthTablePicker {
            id: monthTable
            onClicked: {
                editDialog.close();
                root.selectedDate = selectedDate;
                root.editingFinished();
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
                monthTable.set(selectedDate)
                editDialog.open()
            }
        }
    }

    height: label.height;
    Layout.fillWidth: true;

}
