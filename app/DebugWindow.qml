import QtQuick
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine

Rectangle {
    property int menu_height: map.height - 30 - tool_bar.height
    property int menu_width: 320

    //signal light_intensity_changed(newValue: float)

    id: debugMenu
    width: menu_width
    height: menu_height
    z: mouseArea.drag.active ||  mouseArea.pressed ? 200 : 1
    color:  Qt.alpha(Material.backgroundColor, 0.7)
    border { width:3; color:Qt.alpha( "white", 0.5); }
    radius: 10

    Connections {
        target: map
        function onHud_visible_changed(hud_visible) {
            debugMenu.visible = hud_visible;
        }
    }

    x: 10
    y: 0 + tool_bar.height + 10
    Drag.active: mouseArea.drag.active
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        drag.target: parent
    }
    ScrollView {
        x: 10
        y: 10

        height: menu_height - 20
        clip: true
        ColumnLayout {

            Rectangle {
                Layout.fillWidth: true;
                Layout.columnSpan: 2;
                color: Qt.alpha("white", 0.5)
                height: 35
                border { width:1; color:Qt.alpha( "white", 0.5); }
                radius: 5
                Label {
                    x: 10; y: 6
                    text: "Debug Menu"
                    color: "black"
                    font.pixelSize:18
                    font.bold: true
                }
            }

            GridLayout {
                columns: 2

                Label {
                    text: "Detail:"
                }
                Slider {
                    id: lod_slider_debug
                    from: 0.1
                    to: 2.0
                    stepSize: 0.1
                    Layout.fillWidth: true;
                    Component.onCompleted: {
                        lod_slider_debug.value = map.render_quality
                        map.render_quality = Qt.binding(function() { return lod_slider_debug.value })
                    }
                }

                Label { text: "Wireframe:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: ["disabled", "with shading", "white"]
                    Component.onCompleted:  currentIndex = map.shared_config.wireframe_mode;
                    onCurrentValueChanged:  map.shared_config.wireframe_mode = currentIndex;
                }

                Label { text: "Normals:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: ["per Fragment", "Finite-Difference"]
                    currentIndex: 1
                    Component.onCompleted:  currentIndex = map.shared_config.normal_mode;
                    onCurrentValueChanged:  map.shared_config.normal_mode = currentIndex;
                }


                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "Curtains"
                        font.pixelSize:16
                    }
                }
                Label { text: "Mode:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: ["Off", "Normal", "Highlighted", "Hide Rest"]
                    currentIndex: 1
                    Component.onCompleted:  currentIndex = map.shared_config.curtain_settings.x;
                    onCurrentValueChanged:  map.shared_config.curtain_settings.x = currentIndex;
                }
                Label { text: "Height:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: ["Fixed", "Automatic"];
                    currentIndex: 1
                    Component.onCompleted:  currentIndex = map.shared_config.curtain_settings.y;
                    onCurrentValueChanged:  map.shared_config.curtain_settings.y = currentIndex;
                }
                Label { text: "Ref.-Height:" }
                Slider {
                    from: 1.0
                    to: 500.0
                    stepSize: 1.0
                    Layout.fillWidth: true;
                    Component.onCompleted: this.value = map.shared_config.curtain_settings.z;
                    onMoved: map.shared_config.curtain_settings.z = this.value;
                }

                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "Height-Lines"
                        font.pixelSize:16
                    }
                    CheckBox {
                        x: menu_width - this.width - 30; y: -10
                        Component.onCompleted: this.checked = map.shared_config.height_lines_enabled;
                        onCheckStateChanged: map.shared_config.height_lines_enabled = this.checked;
                    }
                }

                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "Phong-Shading"
                        font.pixelSize:16
                    }
                    CheckBox {
                        checked: true
                        x: menu_width - this.width - 30
                        y: -10
                        Component.onCompleted: {
                            this.checked = map.shared_config.phong_enabled;
                        }
                        onCheckStateChanged: {
                            map.shared_config.phong_enabled = this.checked;
                        }
                    }
                }

                Label { text: "Light-Color:" }
                RowLayout {
                    Label {
                        padding: 5
                        Layout.fillWidth: true;
                        Rectangle{
                            anchors.fill: parent
                            color: "transparent"
                            border { width:1; color:Qt.alpha( "white", 0.5); }
                            radius: 5
                        }
                        MouseArea{
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: colorDialog.open_for(parent)
                        }
                        Component.onCompleted: {
                            var tmp = map.shared_config.sun_light;
                            color = Qt.rgba(tmp.x, tmp.y, tmp.z, 1.0);
                        }
                        onColorChanged: {
                            text = color.toString();
                            map.shared_config.sun_light = Qt.vector4d(color.r, color.g, color.b, map.shared_config.sun_light.w)
                        }
                    }
                }

                Label { text: "Light-Intensity:" }
                Slider {
                    from: 0.00
                    to: 1.00
                    Layout.fillWidth: true
                    Component.onCompleted: this.value = map.shared_config.sun_light.w;
                    onMoved: map.shared_config.sun_light.w = this.value;

                }

                Label { text: "Amb.-Color:" }
                RowLayout {
                    Label {
                        padding: 5
                        Layout.fillWidth: true;
                        Rectangle{
                            anchors.fill: parent
                            color: "transparent"
                            border { width:1; color:Qt.alpha( "white", 0.5); }
                            radius: 5
                        }
                        MouseArea{
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: colorDialog.open_for(parent)
                        }
                        Component.onCompleted: {
                            var tmp = map.shared_config.amb_light;
                            color = Qt.rgba(tmp.x, tmp.y, tmp.z, 1.0);
                        }
                        onColorChanged: {
                            text = color.toString();
                            map.shared_config.amb_light = Qt.vector4d(color.r, color.g, color.b, map.shared_config.amb_light.w);
                        }
                    }
                }

                Label { text: "Amb.-Intensity:" }
                Slider {
                    from: 0.01
                    to: 1.00
                    value: 0.5
                    Layout.fillWidth: true
                    Component.onCompleted: this.value = map.shared_config.amb_light.w;
                    onMoved: map.shared_config.amb_light.w = this.value;
                }

                Label { text: "Light-Direction:" }
                RowLayout {
                    function update_sun_position() {
                        let phi = sun_phi.value * Math.PI / 180.0;
                        let theta = sun_theta.value * Math.PI / 180.0;
                        let dir = Qt.vector3d(-Math.sin(theta) * Math.cos(phi), -Math.sin(theta) * Math.sin(phi), -Math.cos(theta));
                        dir = dir.normalized();
                        map.shared_config.sun_light_dir = Qt.vector4d(dir.x, dir.y, dir.z, 1.0);
                    }
                    Dial {
                        id: sun_phi
                        implicitHeight: 75
                        implicitWidth: 70
                        stepSize: 5
                        value: 135
                        wrap: true
                        onValueChanged: {
                            children[0].text = value + "°";
                            parent.update_sun_position();
                        }
                        from: 0
                        to: 360
                        snapMode: Dial.SnapOnRelease
                        Label {
                            x: parent.width / 2 - this.width / 2
                            y: parent.height / 2 - this.height / 2
                            text: parent.value + "°"
                        }
                    }
                    Slider {
                        id: sun_theta
                        from: -110
                        to: 110
                        implicitHeight: 75
                        orientation: Qt.Vertical
                        value: 45
                        onValueChanged: {
                            children[0].text = value + "°";
                            parent.update_sun_position();
                        }
                        snapMode: Slider.SnapOnRelease
                        stepSize: 5
                        Label {
                            x: 40
                            y: parent.height / 2 - this.height / 2
                            text: parent.value + "°"
                        }

                    }
                    Component.onCompleted: update_sun_position()
                }



                Label { text: "Material:" }
                Label {
                    property bool initialized: false
                    padding: 5
                    Layout.fillWidth: true;
                    Rectangle{
                        anchors.fill: parent
                        color: "transparent"
                        border { width:1; color:Qt.alpha( "white", 0.5); }
                        radius: 5
                    }
                    MouseArea{
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: colorDialog.open_for(parent)
                    }
                    Component.onCompleted: {
                        var tmp = map.shared_config.material_color;
                        color = Qt.rgba(tmp.x, tmp.y, tmp.z, 1.0);
                        initialized = true;
                        onColorChanged();
                    }
                    onColorChanged: {
                        if (initialized) {
                            text = color.toString();
                            map.shared_config.material_color = Qt.vector4d(color.r, color.g, color.b, map.shared_config.material_color.w)
                        }
                    }
                }


                Label { text: "Ortho-Mix:" }
                Slider {
                    id: slider_ortho_mix
                    from: 0.0
                    to: 1.0
                     Layout.fillWidth: true;
                     Component.onCompleted: value = map.shared_config.material_color.w;
                     onMoved: map.shared_config.material_color.w = value;
                }

                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "SSAO"
                        font.pixelSize:16
                    }
                    CheckBox {
                        x: menu_width - this.width - 30
                        y: -10
                        Component.onCompleted: this.checked = map.shared_config.ssao_enabled;
                        onCheckStateChanged: map.shared_config.ssao_enabled = this.checked;
                    }
                }

                Label { text: "Kernel-Size:" }
                Slider {
                    from: 5
                    to: 64
                    Layout.fillWidth: true;
                    Component.onCompleted: value = map.shared_config.ssao_kernel;
                    onMoved: map.shared_config.ssao_kernel = value;
                }

                Label { text: "Falloff-To:" }
                Slider {
                    from: 0
                    to: 100
                    Layout.fillWidth: true;
                    Component.onCompleted: value = map.shared_config.ssao_falloff_to_value * 100;
                    onMoved: map.shared_config.ssao_falloff_to_value = value / 100;
                }

                Label { text: "Blur-Size:" }
                Slider {
                    from: 0
                    to: 2
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    Layout.fillWidth: true;
                    Component.onCompleted: value = map.shared_config.ssao_blur_kernel_size;
                    onMoved: map.shared_config.ssao_blur_kernel_size = value;
                }


                CheckBox {
                    text: "Range-Check"
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    Component.onCompleted: this.checked = map.shared_config.ssao_range_check;
                    onCheckStateChanged: map.shared_config.ssao_range_check = this.checked;
                }

                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "CSM"
                        font.pixelSize:16
                    }
                    CheckBox {
                        x: menu_width - this.width - 30
                        y: -10
                        Component.onCompleted: this.checked = map.shared_config.csm_enabled;
                        onCheckStateChanged: map.shared_config.csm_enabled = this.checked;
                    }
                }

                CheckBox {
                    text: "Overlay Shadow-Maps"
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    Component.onCompleted: this.checked = map.shared_config.overlay_shadowmaps;
                    onCheckStateChanged: map.shared_config.overlay_shadowmaps = this.checked;
                }



                Rectangle {
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
                    color: Qt.alpha("white", 0.1)
                    height: 30
                    border { width:1; color:Qt.alpha( "white", 0.5); }
                    radius: 5
                    Label {
                        x: 10; y: 3
                        text: "Debug"
                        font.pixelSize:16
                    }
                }

                Label { text: "Overlay:" }
                ComboBox {
                    Layout.fillWidth: true;
                    model: ["None", "Ortho-Picture", "Normals", "Tiles", "Zoomlevel", "Vertex-ID", "Vertex Height-Sample", "SSAO Buffer"]
                    Component.onCompleted:  currentIndex = map.shared_config.debug_overlay;
                    onCurrentValueChanged:  map.shared_config.debug_overlay = currentIndex;
                }

                Label { text: "Strength:" }
                Slider {
                    id: slider_overlay_strength
                    from: 0.0
                    to: 1.0
                     Layout.fillWidth: true;
                     Component.onCompleted: this.value = map.shared_config.debug_overlay_strength;
                     onMoved: map.shared_config.debug_overlay_strength = value;
                }
            }
        }
    }

    ColorDialog {
        property Label target
        id: colorDialog
        options: ColorDialog.ShowAlphaChannel
        function open_for(target) {
            selectedColor = target.color;
            this.target = target;
            this.open();
        }
        onAccepted: {
            target.color = selectedColor
        }
    }
}

