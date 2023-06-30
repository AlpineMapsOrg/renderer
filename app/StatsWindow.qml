import QtQuick
import QtCharts 2.5
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

Rectangle {
    property int infomenu_height: 380
    property int infomenu_width: 270

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

    Pane {
        id: graph_dialog
        x: statsMenu.width
        y: -10
        z: 1
        visible: false
        background: transparent

        NumberAnimation on opacity {
            id: in_animation
            running: false
            from: 0; to: 1
            easing.type: Easing.OutCurve; duration: 500
        }
        NumberAnimation on opacity {
            id: out_animation
            running: false
            from: 1; to: 0
            easing.type: Easing.InCurve; duration: 500
            onFinished: graph_dialog.visible = false;
        }

        Rectangle {
            anchors.fill: parent;
            color:  Qt.alpha(Material.backgroundColor, 0.7)
            border { width:3; color:Qt.alpha( "white", 0.5); }
            implicitWidth: 400
            implicitHeight: 400
            radius: 10
            ChartView {
                id: piechart
                anchors.fill: parent;
                legend.alignment: Qt.AlignBottom
                legend.visible: false
                visible: false
                backgroundColor: "transparent"
                antialiasing: true
                theme: ChartView.ChartThemeDark
                PieSeries {
                    id: pieSeries
                    size: 0.65
                }
            }
            ChartView {
                id: linechart
                anchors.fill: parent;
                backgroundColor: "transparent"
                antialiasing: true
                theme: ChartView.ChartThemeDark
                LineSeries {
                    id: lineSeries
                    name: "LineSeries"
                }
            }

            Rectangle {
                anchors.left: parent.left;
                anchors.right: parent.right;
                height: 40;
                color: Qt.alpha( "white", 0.2);
                Label {
                    id: dialog_title
                    x: 15; y: 8
                    text: "Statistics"
                    font.pixelSize:18
                    font.bold: true
                }
                radius: 10
            }

            Rectangle {
                anchors.left: parent.left;
                height: 80;
                width: 20;
                y: parent.height / 2 - height / 2;
                color: Qt.alpha( "white", 0.5);
                radius: 2
                opacity: 0.5;
                Label {
                    text: "<\n<\n<"
                    font.pixelSize:12
                    font.bold: true
                    x: 7
                    y: 16
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor;
                    onEntered: parent.opacity = 0.8;
                    onExited: parent.opacity = 0.5;
                    onClicked: graph_dialog.hideGraph();
                }

            }

        }

        function showPieGraph(title, labels, values, colors) {
            dialog_title.text = title;
            piechart.visible = true;
            linechart.visible = false;
            pieSeries.clear();
            var sum = 0.0;
            for (var i = 0; i < labels.length; i++) sum += values[i];
            for (i = 0; i < labels.length; i++) {
                pieSeries.append(labels[i], values[i]);
                pieSeries.at(i).labelPosition = PieSlice.LabelOutside;
                pieSeries.at(i).labelVisible = true;
                pieSeries.at(i).label = (values[i] / sum * 100).toFixed(2) + " %";
                pieSeries.at(i).color = colors[i];
            }
            if (!graph_dialog.visible) {
                graph_dialog.visible = true;
                in_animation.start();
            }
        }

        function showLineGraph(title, valuesX, valuesY, color) {
            dialog_title.text = title;
            piechart.visible = false;
            linechart.visible = true;
            lineSeries.clear();
            lineSeries.color = color;
            var minY = 1000, maxY = -1;
            for (var i = 0; i < valuesX.length; i++) {
                minY = Math.min(minY, valuesY[i]);
                maxY = Math.max(maxY, valuesY[i]);
                lineSeries.append(valuesX[i], valuesY[i]);
            }
            lineSeries.axisX.max = i;
            lineSeries.axisY.max = maxY;
            lineSeries.axisY.min = minY;
            linechart.update();
            linechart.axisY(lineSeries);
            if (!graph_dialog.visible) {
                graph_dialog.visible = true;
                in_animation.start();
            }
        }

        function hideGraph() {
            out_animation.start();
        }
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
                Layout.preferredWidth: infomenu_width - 20;
                //width: infomenu_width - 20
                color: Qt.alpha("white", 0.5)
                height: 35
                border { width:1; color:Qt.alpha( "white", 0.5); }
                radius: 5
                Label {
                    x: 10; y: 6
                    text: "Statistics"
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
                    text: "Frame-Profiler"
                    font.pixelSize:16
                }
            }

            Pane {
                //Layout.preferredHeight: 50;
                Layout.fillWidth: true;
                background: transparent;
                id: stats_timing;
                ColumnLayout {
                    id: stats_timing_layout;
                }

                Timer {
                    id: dialog_refresh_delay_timer;
                    interval: 100; running: false; repeat: false;
                    onTriggered: stats_timing.refresh_dialog();
                }

                property variant items: ({})
                property variant groups: ({})
                property variant currently_open_entity: null

                function show_group_dialog(group_name) {
                    var labels = [], values = [], colors = [];
                    for (var key in items) {
                        var item = items[key];
                        if (item.element.group === group_name) {
                            labels.push(item.element.name);
                            values.push(item.element.average);
                            colors.push(item.element.color);
                        }
                    }
                    graph_dialog.showPieGraph(group_name + " timing-group" , labels, values, colors);
                    currently_open_entity = groups[group_name];
                }

                function show_timer_dialog(timer_name) {
                    var xvalues = [], yvalues = [];
                    var ticks = items[timer_name].element.measurements;
                    for (var i = 0 ; i < ticks.length; i++) {
                        xvalues.push(i);
                        yvalues.push(ticks[i]);
                    }
                    graph_dialog.showLineGraph(timer_name + " timing", xvalues, yvalues, items[timer_name].element.color);
                    currently_open_entity = items[timer_name];
                }

                function refresh_dialog() {
                    if (graph_dialog.visible) {
                        if (currently_open_entity.hasOwnProperty("groupName")) {
                            show_group_dialog(currently_open_entity.groupName);
                        } else {
                            show_timer_dialog(currently_open_entity.element.name);
                        }
                    }
                }

                function create_group_gui_object(name) {
                    const groupLabel = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; import QtQuick.Layouts;
                                                            Label {
                                                                text: "` + name + `:";
                                                                font.pixelSize:14;
                                                                font.bold: true;

                                                          MouseArea {
                                                              anchors.fill: parent;
                                                          hoverEnabled: true;
                                                          cursorShape: Qt.PointingHandCursor;
                                                          onEntered: parent.font.underline = true;
                                                          onExited: parent.font.underline = false;
                                                            onClicked: { stats_timing.show_group_dialog("` + name + `") }
                                                          }
                                                            }`, stats_timing_layout, "dynamic");
                    const groupContainer = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; import QtQuick.Layouts;
                                                            GridLayout {
                                                                columns: 3
                                                                columnSpacing: 5
                                                                Layout.fillWidth: true;
                                                            }`, stats_timing_layout, "dynamic");
                    groups[name] = {
                        groupName: name,
                        groupLabel: groupLabel,
                        groupContainer: groupContainer
                    };
                }

                function create_timing_gui_object(ele) {
                    if (!groups.hasOwnProperty(ele.group)) {
                        // Group does not exist
                        create_group_gui_object(ele.group);
                    }
                    const groupContainer = groups[ele.group].groupContainer;

                    const colorIndicatorObject = Qt.createQmlObject(`import QtQuick; Rectangle {width: 10; height: 10}`, groupContainer, "dynamic");
                    colorIndicatorObject.color = ele.color;
                    const labelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label {text: "Atmosphere: "}`, groupContainer, "dynamic");
                    labelObject.text = ele.name;
                    const timeLabelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label { font.bold: true; text: "1.23 (Ø 2.34) [ms]"; MouseArea {
                                                               anchors.fill: parent;
                                                           hoverEnabled: true;
                                                           cursorShape: Qt.PointingHandCursor;
                                                           onEntered: parent.font.underline = true;
                                                           onExited: parent.font.underline = false;
                                                             onClicked: stats_timing.show_timer_dialog("` + ele.name + `");
                                                           }}`, groupContainer, "dynamic");
                    items[ele.name] = {
                        colorIndicatorObject: colorIndicatorObject,
                        labelObject: labelObject,
                        timeLabelObject: timeLabelObject,
                        element: ele
                    };
                }

                function add_or_refresh_element(ele) {
                    if (!items.hasOwnProperty(ele.name)) {
                        // Create new gui object
                        create_timing_gui_object(ele);
                    }
                    items[ele.name].timeLabelObject.text = ele.last_measurement.toFixed(2) + " (Ø " + ele.average.toFixed(2) + ") [ms]";
                }

                Connections {
                    target: map.timer_manager
                    // Gets invoked whenever new frame time data is available
                    function onUpdateTimingList(data) {
                        for (var i = 0; i < data.length; i++) {
                            stats_timing.add_or_refresh_element(data[i]);
                        }
                        if (!dialog_refresh_delay_timer.running)
                            dialog_refresh_delay_timer.start();
                    }
                }

            }

            CheckBox {
                text: "Benchmark Mode"
            }

        }

    }

}


