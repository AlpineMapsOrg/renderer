import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Alpine

Item {
    property int theme: Material.System
    property int accent: Material.Pink

    Rectangle {
        id: tool_bar
        height: 60
        color: main_stack_view.depth === 1 ? "#00FF00FF" : Qt.alpha(Material.backgroundColor, 0.7)
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
                text: ""
                visible: menu_list_view.currentIndex !== 0
                wrapMode: Label.Wrap
                font.pointSize: 24
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
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 10
        }
    }

    Drawer {
        id: menu
        width: Math.min(parent.width, parent.height) / 3 * 2
        height: parent.height
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
                    menu.change_page(index)
                }
            }

            model: ListModel {
                ListElement { title: qsTr("Map"); source: "map" }
                ListElement { title: qsTr("Coordinates"); source: "/app/Coordinates.qml" }
//                ListElement { title: qsTr("Cached Content"); source: "" }
                ListElement { title: qsTr("Settings"); source: "/app/Settings.qml" }
                ListElement { title: qsTr("About"); source: "/app/About.qml" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        function change_page(index) {
            menu_list_view.currentIndex = index
            var model = menu_list_view.model.get(index)

            if (model.source === "map") {
                if (main_stack_view.depth >= 1)
                    main_stack_view.pop()
                menu.close()
                return;
            }

            if (main_stack_view.depth === 1)
                main_stack_view.push(_qmlPath + model.source, {renderer: map})
            else
                main_stack_view.replace(_qmlPath + model.source, {renderer: map})
            page_title.text = model.title
            menu.close()
        }
    }
    Component.onCompleted: menu.change_page(0)

    TerrainRenderer {
        id: map
        focus: true
        anchors.fill: parent
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
}

