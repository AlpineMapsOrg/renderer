import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

import "components"

Item {
    property int theme: Material.Light //Material.System
    property int accent: Material.Orange

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

        iconTitle: "Alpine Maps"
        iconSource: "../icons/favicon_256.png"
        iconSubtitle: qsTr ("Version 0.8 Alpha")

        actions: {
            0: function() { change_page("map", qsTr("Map")) },
            1: function() { change_page("Coordinates.qml", qsTr("Coordinates")) },
            2: function() { change_page("Settings.qml", qsTr("Settings")) },
            5: function() { change_page("About.qml", qsTr("About")) }
        }

        items: ListModel {
            id: pagesModel

            ListElement {
                pageTitle: qsTr ("Map")
                pageIcon: "../icons/minimal/mountain.png"
            }

            ListElement {
                pageTitle: qsTr ("Coordinates")
                pageIcon: "../icons/minimal/location.png"
            }

            ListElement {
                pageTitle: qsTr ("Settings")
                pageIcon: "../icons/minimal/settings.png"
            }

            ListElement {
                spacer: true
            }

            ListElement {
                separator: true
            }

            ListElement {
                pageTitle: qsTr ("About")
                pageIcon: "../icons/minimal/information.png"
            }
        }
    }

    function change_page(source, title) {
        if (source === "map") {
            if (main_stack_view.depth >= 1) main_stack_view.pop()
            menu.close()
            page_title.visible = false
            search.visible = true
            return
        }
        if (main_stack_view.depth === 1)
            main_stack_view.push(_qmlPath + source, {renderer: map})
        else
            main_stack_view.replace(_qmlPath + source, {renderer: map})
        page_title.visible = true
        search.visible = false
        page_title.text = title
        menu.close()
    }

    TerrainRenderer {
        id: map
        focus: true
        anchors.fill: parent
        onHud_visible_changed: function(new_hud_visible) {
            tool_bar.visible = new_hud_visible;
        }
    }

    StackView {
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

    /*
    DebugWindow {}

    StatsWindow {}
*/
     //property TerrainRenderer renderer
    Component.onCompleted: {
        menu.change_page(0)
    }

}

