import QtQuick
import QtCharts 2.5
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

Rectangle {
    property int infomenu_height: map.height - 30 - tool_bar.height -100
    property int infomenu_width: 270

    id: statsMenu
    width: infomenu_width
    height: infomenu_height
    z: statsMenuMouse.drag.active ||  statsMenuMouse.pressed ? 200 : 1
    color:  Qt.alpha(Material.backgroundColor, 0.7)
    border { width:3; color:Qt.alpha( "white", 0.5); }
    radius: 10

    x: map.width - infomenu_width - 10
    y: 0 + tool_bar.height + 10
    Drag.active: statsMenuMouse.drag.active
    MouseArea {
        id: statsMenuMouse
        anchors.fill: parent
        drag.target: parent
    }

    Pane {
        id: graph_dialog
        x: - this.width
        y: -10
        z: 1
        visible: false
        background: transparent

        property string type: 'line';
        onTypeChanged: {
            if (type === 'line') {
                piechart.visible = false;
                linechart.visible = true;
                refreshLineGraph();
            } else if (type === 'pie') {
                piechart.visible = true;
                linechart.visible = false;
                refreshPieGraph();
            }
        }

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
                width: parent.width;
                height: parent.height - 20;
                y: 20;
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
                width: parent.width;
                height: parent.height - 20;
                y: 20;
                backgroundColor: "transparent"
                legend.visible: false
                antialiasing: true
                axes: [
                    ValueAxis {
                        id: xAxis; min: 0.0; max: 120.0;
                    },
                    ValueAxis {
                        id: yAxis; min: 0.0; max: 100.0;
                    }
                ]
                theme: ChartView.ChartThemeDark
            }

            Label {
                id: dialog_title
                x: 15; y: 10
                text: "Frame-Graph"
                font.pixelSize:18
                font.bold: true;
                opacity: 0.8
            }

            RowLayout {
                x: parent.width - width - 5;
                y: 5;

                Rectangle {
                    id: cb_avg;
                    property bool checked: true;
                    height: 30; width: 30; radius: 20;
                    color: checked ? Qt.alpha("white", 0.1) : "transparent";
                    Label {
                        text: "⌀"
                        font.family: "Helvetica"
                        font.pixelSize:30
                        color: parent.checked ? "orange" : "white";
                        x: parent.width / 2 - width / 2;
                        y: parent.height / 2 - height / 2 - 2;
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor;
                        onEntered: parent.color = Qt.alpha( "white", 0.5);
                        onExited: parent.color = parent.checked ? Qt.alpha("white", 0.1) : "transparent";
                        onClicked: {
                            parent.checked = !parent.checked;
                            graph_dialog.refreshGraph();
                        }
                    }
                }

                Rectangle {
                    height: 30; width: 30; radius: 20;
                    color: "transparent";
                    Label {
                        font.family: "Webdings"
                        text: "y"
                        font.pixelSize:24
                        x: parent.width / 2 - width / 2;
                        y: parent.height / 2 - height / 2;
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor;
                        onEntered: parent.color = Qt.alpha( "white", 0.5);
                        onExited: parent.color = "transparent";
                        onClicked: {
                            let newType = graph_dialog.type === "pie" ? "line" : "pie";
                            graph_dialog.type = newType;
                            parent.children[0].text = newType === "pie" ? "" : "y";
                        }
                    }
                }


            }



        }

        function refreshGraph() {
            // Check wether graph should be visible
            if (Object.keys(stats_timing.open_timers).length > 0) {
                if (!visible) {
                    graph_dialog.visible = true;
                    in_animation.start();
                }
            } else if (visible) {
                out_animation.start();
                return;
            }

            // refresh data:
            if (type === 'line') {
                refreshLineGraph();
            } else if (type === 'pie') {
                refreshPieGraph();
            }
        }

        function refreshLineGraph() {
            linechart.removeAllSeries();
            var minY = 1000, maxY = -1;
            var minX = 1000000, maxX = -1;
            for (var key in stats_timing.open_timers) {
                let tmr = stats_timing.open_timers[key].element;
                let series = linechart.createSeries(ChartView.SeriesTypeLine, tmr.name, xAxis, yAxis);
                series.color = tmr.color;
                for (var i = 0; i < tmr.measurements.length; i++) {
                    let val = tmr.measurements[i];
                    let x = val.x;
                    let y = cb_avg.checked ? val.z : val.y;
                    minY = Math.min(minY, y); maxY = Math.max(maxY, y);
                    maxX = Math.max(maxX, x); minX = Math.min(minX, x);
                    series.append(x, y);
                }
            }
            xAxis.min = minX; xAxis.max = maxX;
            yAxis.max = maxY; yAxis.min = minY;
        }

        function refreshPieGraph() {
            pieSeries.clear();
            var sum = 0.0;
            var values = {};
            for (var key in stats_timing.open_timers) {
                values[key] = cb_avg.checked ? stats_timing.open_timers[key].element.average : stats_timing.open_timers[key].element.last_measurement;
                sum += values[key];
            }
            var i = 0;
            for (key in stats_timing.open_timers) {
                let tmr = stats_timing.open_timers[key].element;
                pieSeries.append(tmr.name, values[key]);
                pieSeries.at(i).labelPosition = PieSlice.LabelOutside;
                pieSeries.at(i).labelVisible = true;
                pieSeries.at(i).label = (values[key] / sum * 100).toFixed(2) + " %";
                pieSeries.at(i).color = tmr.color;
                i++;
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

                // Timer to decouple graph refreshes from the current frame time. (It's not necessary to update the graph at the same speed)
                Timer {
                    id: dialog_refresh_delay_timer;
                    interval: 100; running: false; repeat: false;
                    onTriggered: stats_timing.refresh_dialog();
                }

                property variant items: ({})
                property variant groups: ({})
                property variant currently_open_entity: null
                property variant open_timers: ({})

                function timer_checkbox_changed() {
                    // Go through all the checkboxes and build current open_timers array
                    open_timers = {};
                    for (var key in items) if (items[key].colorIndicatorObject.checked === true) open_timers[key] = items[key];
                    graph_dialog.refreshGraph();
                }

                function group_clicked(name) {
                    // Check wether all of the timers of the group are already checked:
                    var sum_timers_in_group = 0;
                    for (var key in items) if (items[key].element.group === name) sum_timers_in_group++;
                    for (key in open_timers) if (open_timers[key].element.group === name) sum_timers_in_group--;
                    if (sum_timers_in_group > 0) { // not all timers are active -> activate all
                        for (key in items) if (items[key].element.group === name) items[key].colorIndicatorObject.checked = true;
                    } else { // all timers of this group are already activated -> deactivate all
                        for (key in items) if (items[key].element.group === name) items[key].colorIndicatorObject.checked = false;
                    }
                    timer_checkbox_changed();
                }

                function refresh_dialog() {
                    graph_dialog.refreshGraph();
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
                                                          onClicked: { stats_timing.group_clicked("` + name + `") }
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

                    const colorIndicatorObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material;
                                                                    CheckBox {
                                                                    property int size: 12;
                                                                    property color color: "red";
                                                                    implicitWidth: size; implicitHeight: size;
                                                                    onCheckStateChanged: indicator.color = checked ? color : Qt.alpha( "white", 0.5);
                                                                    indicator: Rectangle {
                                                                        implicitWidth: parent.size; implicitHeight: parent.size; radius: 3;
                                                                        color: parent.checked ? parent.color : Qt.alpha( "white", 0.5);
                                                                        border.color: "white";
                                                                        MouseArea { anchors.fill: parent;
                                                                            hoverEnabled: true;
                                                                            cursorShape: Qt.PointingHandCursor;
                                                                            onEntered: parent.color = parent.parent.checked ? parent.parent.color : Qt.alpha(parent.parent.color, 0.5);
                                                                            onExited: parent.color = parent.parent.checked ? parent.parent.color : Qt.alpha( "white", 0.5);
                                                                            onClicked: {
                                                                                parent.parent.checked = !parent.parent.checked;
                                                                                parent.color = parent.parent.checked ? parent.parent.color : Qt.alpha(parent.parent.color, 0.5);
                                                                                stats_timing.timer_checkbox_changed();
                                                                            }
                                                                        }
                                                                    }
                                                                }`, groupContainer, "dynamic");
                    colorIndicatorObject.color = ele.color;
                    const labelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label {text: "Atmosphere: "}`, groupContainer, "dynamic");
                    labelObject.text = ele.name;
                    const timeLabelObject = Qt.createQmlObject(`import QtQuick; import QtQuick.Controls.Material; Label { font.bold: true; text: "1.23 (Ø 2.34) [ms]"; }`, groupContainer, "dynamic");
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
                    items[ele.name].timeLabelObject.text = ele.last_measurement.toFixed(2) + " (Ø " + ele.quick_average.toFixed(2) + ") [ms]";
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
                onCheckStateChanged: map.render_looped = checked
            }

            GridLayout {
                columns: 2

                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "Camera"
                        font.pixelSize:16
                    }
                }

                Label { text: "Cam.-Pos.:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: _positionList    // set in main.cpp
                    currentIndex: 0
                    onCurrentIndexChanged: map.selected_camera_position_index = currentIndex;
                }

            }
        }


    }

}


