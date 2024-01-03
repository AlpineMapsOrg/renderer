/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celerek
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
import QtQuick.Dialogs
import Alpine

import "components"

Item {
    id: main
    property int theme: Material.Light      //Material.System
    property int accent: Material.Green
    property string selectedPage: "map";


    Rectangle {
        id: tool_bar
        height: 60
        color: main_stack_view.depth === 1 ? "#01FF00FF" : Qt.alpha(Material.backgroundColor, 0.7)
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
                    source: "icons/menu.svg"
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
                text: ""
                wrapMode: Label.Wrap
                font.pointSize: 24
                Layout.fillWidth: true
            }
            SearchBox {
                id: search
                search_results: search_results
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
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 10
        }
    }

    PageDrawer {
        id: menu

        bannerTitle: "Alpine Maps"
        bannerIconSource: "../icons/favicon_256.png"
        bannerSubtitle: _alpine_renderer_version
        selectedButtonId: 0

        DrawerSeparator {}

        DrawerButton {
            bid: 0
            text: qsTr ("Map")
            iconSource: "../icons/material/map.svg"
            onClicked: change_page("map", qsTr("Map"))
        }

        DrawerButton {
            text: qsTr ("Coordinates")
            iconSource: "../icons/material/pin_drop.svg"
            onClicked: change_page("Coordinates.qml", qsTr("Coordinates"))
        }

        DrawerButton {
            text: qsTr ("Settings")
            iconSource: "../icons/material/settings.svg"
            onClicked: change_page("Settings.qml", qsTr("Settings"))
        }

        DrawerSeparator {}

        DrawerButton {
            text: qsTr("Reload Shaders")
            iconSource: "../icons/material/3d_rotation.svg"
            hotkey: "F6"
            selectable: false
            onClicked: map.reload_shader();
        }

        DrawerButton {
            text: stats_window.visible ? qsTr ("Hide Statistics") : qsTr("Statistics")
            hotkey: "F8"
            iconSource: "../icons/material/monitoring.svg"
            selectable: false
            onClicked: toggleStatsWindow();
        }

        DrawerButton {
            text: qsTr("Hide User Interface")
            iconSource: "../icons/material/visibility_off.svg"
            hotkey: "F10"
            selectable: false
            onClicked: map.hud_visible = false;
        }

        DrawerSpacer {}

        DrawerSeparator {}

        DrawerButton {
            text: qsTr ("About")
            iconSource: "../icons/material/info.svg"
            onClicked: change_page("About.qml", qsTr("About"))
        }

    }

    function change_page(source, title) {
        selectedPage = source.toLowerCase().replace(".qml", "");
        if (selectedPage !== "map" && selectedPage !== "settings") {
            stats_window.visible = false;
        }
        if (source === "map") {
            if (main_stack_view.depth >= 1) main_stack_view.pop()
            page_title.visible = false
            search.visible = true
            main_stack_view.selectedPage = selectedPage
            return
        }
        if (main_stack_view.depth === 1)
            main_stack_view.push(_qmlPath + source)
        else
            main_stack_view.replace(_qmlPath + source)
        main_stack_view.selectedPage = selectedPage
        page_title.visible = true
        search.visible = false
        page_title.text = title
        main.onWidthChanged(); // trigger responsive updates manually
    }

    function toggleStatsWindow() {
        stats_window.visible = !stats_window.visible
        main.onWidthChanged(); // trigger responsive updates manually
    }


    TerrainRenderer {
        property var allLvl1HudElements: [tool_bar, main_stack_view, stats_window, fab_group]
        property var _hudElementsVisibility: []
        id: map
        focus: true
        anchors.fill: parent
        Keys.onPressed: function(event){
            if (event.key === Qt.Key_F8) {
                toggleStatsWindow();
            }
        }

        onHud_visible_changed: function(new_hud_visible) {
            if (new_hud_visible) { // show all items
                for (let i1 = 0; i1 < allLvl1HudElements.length; i1++) {
                    allLvl1HudElements[i1].visible = _hudElementsVisibility[i1];
                }
            } else { // hide all items and save their state
                _hudElementsVisibility = [];
                for (let i = 0; i < allLvl1HudElements.length; i++) {
                    _hudElementsVisibility.push(allLvl1HudElements[i].visible);
                    allLvl1HudElements[i].visible = false;
                }
            }
        }
    }

    StackView {
        property string selectedPage: "map"
        id: main_stack_view
        anchors {
            top: tool_bar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        initialItem: Map {
            renderer: map
        }
    }

    StatsWindow {
        id: stats_window
        visible: false
    }

    FloatingActionButtonGroup {
        id: fab_group
    }

     //property TerrainRenderer renderer
    Component.onCompleted: {
        change_page("map")
    }

}

