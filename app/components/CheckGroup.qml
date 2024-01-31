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
    default property alias content: groupchildren.children
    property alias name: groupname.text;
    property alias checked: checkbox.checked;
    property bool checkBoxEnabled: false;
    id: root
    Layout.fillWidth: true
    Layout.topMargin: 20
    Layout.preferredHeight: rootlayout.implicitHeight

    onCheckedChanged: {
        if (checked) groupchildren.visible = true
        else groupchildren.visible = false
    }

    ColumnLayout {
        id: rootlayout
        anchors.fill: parent
        RowLayout {
            id: header
            visible: root.name !== ""
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                Layout.preferredHeight: 2
                Layout.preferredWidth: 40
                color: Material.color(Material.Grey)
                CheckBox {
                    id: checkbox
                    visible: root.checkBoxEnabled
                    y: -this.height/2
                    Component.onCompleted: {
                        // NOTE: CAN BREAK IF QT DEFINITION OF CHECKBOX CHANGES
                        indicator.color = Material.color(Material.Grey);
                    }
                }
            }
            Label {
                id: groupname
                text: ""
                font.bold: true
                font.capitalization: Font.AllUppercase
                Layout.preferredWidth: implicitWidth + 10
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Qt.alpha(Material.hintTextColor, 0.2)
            }
        }


        GridLayout {
            columns: 2
            id: groupchildren
            Layout.fillWidth: true
        }
    }

}
