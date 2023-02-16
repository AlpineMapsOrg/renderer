/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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
import QtQuick.Controls
import QtQuick.Layouts
import MyRenderLibrary

Window {
    visible: true
    id: root_window

    Rectangle {
        id: tool_bar
        implicitHeight: 60
        color: "#00FF00FF"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 6
        }
        height: 48

        RowLayout {
            anchors.fill: parent
            Rectangle{
                width: 48
                height: 48
                color: "#00FF0000"
                Image {
                    source: "qrc:/alpinemaps/app/icons/menu.svg"
                    width: parent.width / 2
                    height: parent.height / 2
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        menu.open()
                    }
                }
            }
        }
        z: 100
    }

    Drawer {
        id: menu
        width: Math.min(root_window.width, root_window.height) / 3 * 2
        height: root_window.height
        interactive: true

        ListView {
            id: menu_list_view

//            focus: true
//            currentIndex: -1
            anchors.fill: parent

            delegate: ItemDelegate {
                width: menu_list_view.width
                text: model.title
//                highlighted: ListView.isCurrentItem
                onClicked: {
//                    menu_list_view.currentIndex = index
//                    stackView.push(model.source)
                    menu.close()
                }
            }

            model: ListModel {
                ListElement { title: "Cached Content"; component: "" }
                ListElement { title: "Settings"; component: "qrc:/pages/Settings.qml" }
                ListElement { title: "About"; component: "" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    StackView {
        id: main_stack_view
        anchors.fill: parent
        initialItem: Map {
            anchors.fill: parent
        }
    }
}
