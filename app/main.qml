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
import Alpine

Window {
    visible: true
    id: root_window

    Rectangle {
        id: tool_bar
        height: 60
        color: main_stack_view.depth === 1 ? "#00FF00FF" : "#AAFFFFFF"
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
                    source: "qrc:/icons/menu.svg"
                    width: parent.width / 2
                    height: parent.height / 2
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        menu.open()
                        search_results.visible = false
                    }
                }
            }
            Label {
                id: page_title
//                anchors.verticalCenter: parent
                text: ""
                visible: menu_list_view.currentIndex !== 0
                wrapMode: Label.Wrap
//                background: Rectangle { color: "#99FF00FF" }
                font.pointSize: 24
//                font.weight: Font.ExtraBold
                Layout.fillWidth: true
            }
            SearchBox {
                id: search
                search_results: search_results
                visible: menu_list_view.currentIndex === 0
            }
        }
        z: 100
    }

    SearchResults {
        id: search_results
        map: map
        search_height: search.height
        anchors {
            top: tool_bar.bottom
            bottom: root_window.contentItem.bottom
            left: root_window.contentItem.left
            right: root_window.contentItem.right
            margins: 10
        }
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
//                        page_title.text = ""
                        return;
                    }


                    if (main_stack_view.depth === 1)
                        main_stack_view.push(model.source, {renderer: map})
                    else
                        main_stack_view.replace(model.source, {renderer: map})
                    page_title.text = model.title
                    menu.close()
                }
            }

            model: ListModel {
                ListElement { title: qsTr("Map"); source: "map" }
                ListElement { title: qsTr("Coordinates"); source: "qrc:/app/Coordinates.qml" }
//                ListElement { title: qsTr("Cached Content"); source: "" }
                ListElement { title: qsTr("Settings"); source: "qrc:/app/Settings.qml" }
                ListElement { title: qsTr("About"); source: "qrc:/app/About.qml" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    TerrainRenderer {
        id: map
        focus: true
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

        initialItem: Map {
            renderer: map
        }
    }
}
