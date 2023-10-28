import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Alpine

Drawer {
    id: drawer

    implicitHeight: parent.height
    implicitWidth: Math.min (parent.width > parent.height ? 400 : 300,
                             Math.min (parent.width, parent.height) * 0.90)

    property string iconTitle: ""
    property string iconSource: ""
    property string iconSubtitle: ""
    property size iconSize: Qt.size (72, 72)
    property color iconBgColorLeft: "#FFFFFF"
    property color iconBgColorRight: "#ebf8f2"

    property alias items: listView.model
    property alias index: listView.currentIndex

    background: Rectangle {
        anchors.fill: parent
        color:  Qt.alpha(Material.backgroundColor, 0.9)
        radius: 0  // Adjust this value to modify the rounded border
    }
    topPadding: 0
    bottomPadding: 0

    onIndexChanged: {
        var isSpacer = false
        var isSeparator = false
        var item = items.get (index)

        if (typeof (item) !== "undefined") {
            if (typeof (item.spacer) !== "undefined")
                isSpacer = item.spacer

            if (typeof (item.separator) !== "undefined")
                isSpacer = item.separator

            if (!isSpacer && !isSeparator)
                actions [index]()
        }
    }

    property var actions

    //
    // Main layout of the drawer
    //
    ColumnLayout {
        spacing: 0
        anchors.margins: 0
        anchors.fill: parent

        //
        // Icon controls
        //
        Rectangle {
            z: 1
            height: 120
            id: iconRect
            Layout.fillWidth: true

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: iconBgColorLeft  }
                    GradientStop { position: 1.0; color: iconBgColorRight }
                }
            }

            RowLayout {
                spacing: 16

                anchors {
                    fill: parent
                    centerIn: parent
                    margins: 16
                }

                Image {
                    source: iconSource
                    sourceSize: iconSize
                }

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Item {
                        Layout.fillHeight: true
                    }

                    Label {
                        color: "#1b1b1b"
                        text: iconTitle
                        font.weight: Font.Medium
                        font.pixelSize: 24
                    }

                    Label {
                        color: "#1b1b1b"
                        opacity: 0.87
                        text: iconSubtitle
                        font.pixelSize: 14
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }

        //
        // Page selector
        //
        ListView {
            z: 0
            id: listView
            currentIndex: -1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: currentIndex = 0

            delegate: DrawerItem {
                model: items
                width: parent.width
                pageSelector: listView

                onClicked: {
                    if (listView.currentIndex !== index)
                        listView.currentIndex = index

                    drawer.close()
                }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
