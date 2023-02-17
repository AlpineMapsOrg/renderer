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
import QtQuick.Controls
import QtQuick.Layouts
import MyRenderLibrary

Window {
    visible: true
    id: root_window

    Rectangle {
        id: tool_bar
        implicitHeight: 60
        color: main_stack_view.depth == 1 ? "#00FF00FF" : "#AAFFFFFF"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            Rectangle {
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
            Label {
                id: page_title
                text: ""
                wrapMode: Label.Wrap
//                background: Rectangle { color: "#99FF00FF" }
                font.pointSize: 24
                font.weight: Font.ExtraBold
                Layout.fillWidth: true
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
            currentIndex: 0
            anchors.fill: parent

            delegate: ItemDelegate {
                width: menu_list_view.width
                text: model.title
                highlighted: ListView.isCurrentItem
                onClicked: {
                    menu_list_view.currentIndex = index
                    if (model.source === "map") {
                        if (main_stack_view.depth >= 1)
                            main_stack_view.pop()
                        menu.close()
                        page_title.text = ""
                        return;
                    }

                    if (main_stack_view.depth === 1)
                        main_stack_view.push(model.source)
                    else
                        main_stack_view.replace(model.source)
                    page_title.text = model.title
                    menu.close()
                }
            }

            model: ListModel {
                ListElement { title: qsTr("Map"); source: "map" }
//                ListElement { title: qsTr("Cached Content"); source: "" }
                ListElement { title: qsTr("Settings"); source: "qrc:/alpinemaps/app/Settings.qml" }
                ListElement { title: qsTr("About"); source: "qrc:/alpinemaps/app/About.qml" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    MeshRenderer {
        id: map
        anchors.fill: parent
    }

    StackView {
        id: main_stack_view
        anchors {
            top: tool_bar.bottom
            bottom: root_window.contentItem.bottom
            left: root_window.contentItem.left
            right: root_window.contentItem.right
        }

        initialItem: Map {}
    }
}
