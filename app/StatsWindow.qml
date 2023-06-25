import QtQuick
import QtCharts
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

Rectangle {
    property int infomenu_height: 300
    property int infomenu_width: 250

    //signal light_intensity_changed(newValue: float)

    id: statsMenu
    width: infomenu_width
    height: infomenu_height
    z: statsMenuMouse.drag.active ||  statsMenuMouse.pressed ? 200 : 1
    color:  Qt.alpha(Material.backgroundColor, 0.7)
    border { width:3; color:Qt.alpha( "white", 0.5); }
    radius: 10

    x: 10
    y: 0 + tool_bar.height + 10
    Drag.active: statsMenuMouse.drag.active
    MouseArea {
        id: statsMenuMouse
        anchors.fill: parent
        drag.target: parent
    }



    ScrollView {
        x: 10
        y: 10

        ScrollBar.horizontal.interactive: false
        width: infomenu_width - 20
        height: infomenu_height - 20
        clip: true

        ColumnLayout{
            anchors.fill: parent;
            spacing: 5

            Rectangle {
                anchors.right: parent.right;
                anchors.left: parent.left;
                Layout.fillWidth: true;
                //width: infomenu_width - 20
                color: Qt.alpha("white", 0.5)
                height: 35
                border { width:1; color:Qt.alpha( "white", 0.5); }
                radius: 5
                Label {
                    x: 10; y: 6
                    text: "Stats Menu"
                    color: "black"
                    font.pixelSize:18
                    font.bold: true
                }
            }

            Rectangle {
                Layout.fillWidth: true;
                color: Qt.alpha("white", 0.1)
                height: 30
                border { width:1; color:Qt.alpha( "white", 0.5); }
                radius: 5
                Label {
                    x: 10; y: 3
                    text: "Stage Timings"
                    font.pixelSize:16
                }
            }

            GridLayout {
                id: stats_timing_grid
                columns: 3
                columnSpacing: 5
                Layout.fillWidth: true;

                Component.onCompleted: {
                    const colorIndicatorObject = Qt.createQmlObject(`import QtQuick; Rectangle {width: 10; height: 10; id: "timings_color_tiles"}`, stats_timing_grid, "dynamic");
                    colorIndicatorObject.color = "purple"
                    const labelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label {text: "Atmosphere: "}`, stats_timing_grid, "dynamic");
                    labelObject.text = "Tiles";
                    const timeLabelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label { font.bold: true; text: "1.23 (Ø 2.34) [ms]"}`, stats_timing_grid, "dynamic");
                    timeLabelObject.text = "1.23 (Ø 2.34) [ms]";


                }

            }

            CheckBox {
                text: "Benchmark Mode"
            }


        }

    }
}


