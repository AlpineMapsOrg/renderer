import QtQuick
import QtQuick.Controls.Material
//import QtQuick.Controls.Imagine
import QtQuick.Layouts
import QtQuick.Dialogs
import Alpine
import "components"

Rectangle {
    property int menu_height: map.height - 30 - tool_bar.height
    property int menu_width: 320

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
        function onShared_config_changed(conf) {
            wireframe_mode.currentIndex = conf.wireframe_mode;
            normal_mode.currentIndex = conf.normal_mode;
            curtain_settings_mode.currentIndex = conf.curtain_settings.x;
            curtain_settings_height_mode.currentIndex = conf.curtain_settings.y;
            curtain_settings_height_reference.value = conf.curtain_settings.z;
            height_lines_enabled.checked = conf.height_lines_enabled;
            phong_enabled.checked = conf.phong_enabled;
            sun_light_color.color = Qt.rgba(conf.sun_light.x, conf.sun_light.y, conf.sun_light.z, 1.0);
            sun_light_intensity.value = conf.sun_light.w;
            amb_light_color.color = Qt.rgba(conf.amb_light.x, conf.amb_light.y, conf.amb_light.z, 1.0);
            amb_light_intensity.value = conf.amb_light.w;
            material_color.color = Qt.rgba(conf.material_color.x, conf.material_color.y, conf.material_color.z, conf.material_color.w);
            ssao_enabled.checked = conf.ssao_enabled;
            ssao_kernel.value = conf.ssao_kernel;
            ssao_falloff_to_value.value = conf.ssao_falloff_to_value;
            ssao_blur_kernel_size.value = conf.ssao_blur_kernel_size;
            ssao_range_check.checked = conf.ssao_range_check;
            csm_enabled.checked = conf.csm_enabled;
            overlay_shadowmaps.checked = conf.overlay_shadowmaps;
            debug_overlay.currentIndex = conf.debug_overlay;
            debug_overlay_strength.value = conf.debug_overlay_strength;
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
                ValSlider {
                    from: 0.1; to: 2.0; stepSize: 0.1;
                    Component.onCompleted: this.value = map.render_quality;
                    onMoved: map.render_quality = this.value;
                }

                Label { text: "Wireframe:" }
                ComboBox {
                    id: wireframe_mode;
                    Layout.fillWidth: true;
                    model: ["disabled", "with shading", "white"];
                    currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
                    onCurrentIndexChanged:  map.shared_config.wireframe_mode = currentIndex;
                }

                Label { text: "Normals:" }
                ComboBox {
                    id: normal_mode;
                    Layout.fillWidth: true;
                    model: ["per Fragment", "Finite-Difference"];
                    currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
                    onCurrentIndexChanged:  map.shared_config.normal_mode = currentIndex;
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
                    id: curtain_settings_mode;
                    Layout.fillWidth: true;
                    model: ["Off", "Normal", "Highlighted", "Hide Rest"]
                    currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
                    onCurrentIndexChanged:  map.shared_config.curtain_settings.x = currentIndex;
                }
                Label { text: "Height:" }
                ComboBox {
                    id: curtain_settings_height_mode;
                    Layout.fillWidth: true;
                    model: ["Fixed", "Automatic"];
                    currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
                    onCurrentIndexChanged:  map.shared_config.curtain_settings.y = currentIndex;
                }
                Label { text: "Ref.-Height:" }
                ValSlider {
                    id: curtain_settings_height_reference;
                    from: 1.0; to: 500.0; stepSize: 1.0;
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
                        id: height_lines_enabled;
                        x: menu_width - this.width - 30; y: -10
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
                        id: phong_enabled;
                        x: menu_width - this.width - 30;
                        y: -10;
                        onCheckStateChanged: map.shared_config.phong_enabled = this.checked;
                    }
                }

                Label { text: "Light-Colors:" }
                ColorPicker {
                    id: sun_light_color;
                    onColorChanged: map.shared_config.sun_light = Qt.vector4d(color.r, color.g, color.b, map.shared_config.sun_light.w);
                }

                Label { text: "Light-Intensity:" }
                ValSlider {
                    id: sun_light_intensity;
                    from: 0.0; to: 1.0;
                    onMoved: map.shared_config.sun_light.w = this.value;
                }

                Label { text: "Amb.-Color:" }
                ColorPicker {
                    id: amb_light_color;
                    onColorChanged: map.shared_config.amb_light = Qt.vector4d(color.r, color.g, color.b, map.shared_config.amb_light.w);
                }

                Label { text: "Amb.-Intensity:" }
                ValSlider {
                    id: amb_light_intensity;
                    from: 0.01; to: 1.0; stepSize: 0.01;
                    onMoved: map.shared_config.amb_light.w = this.value;
                }

                Label { text: "Light-Direction:" }
                RowLayout {
                    function update_sun_position() {
                        let phi = sun_phi.value * Math.PI / 180.0;
                        let theta = sun_theta.value * Math.PI / 180.0;
                        let dir = Qt.vector3d(-Math.sin(theta) * Math.cos(phi), -Math.sin(theta) * Math.sin(phi), -Math.cos(theta));
                        dir = dir.normalized();
                        console.log("sunlight dir", dir);
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

                Label { text: "Mat.-Color:" }
                ColorPicker {
                    id: material_color;
                    onColorChanged: map.shared_config.material_color = Qt.vector4d(color.r, color.g, color.b, color.a);
                }

                // TODO hier weiter!
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
                        id: ssao_enabled;
                        x: menu_width - this.width - 30
                        y: -10
                        onCheckStateChanged: map.shared_config.ssao_enabled = this.checked;
                    }
                }

                Label { text: "Kernel-Size:" }
                ValSlider {
                    id: ssao_kernel;
                    from: 5; to: 64; stepSize: 1;
                    onMoved: map.shared_config.ssao_kernel = value;
                }

                Label { text: "Falloff-To:" }
                ValSlider {
                    id: ssao_falloff_to_value;
                    from: 0.0; to: 1.0; stepSize: 0.01;
                    onMoved: map.shared_config.ssao_falloff_to_value = value;
                }

                Label { text: "Blur-Size:" }
                ValSlider {
                    id: ssao_blur_kernel_size;
                    from: 0; to: 2; stepSize: 1; snapMode: Slider.SnapAlways;
                    onMoved: map.shared_config.ssao_blur_kernel_size = value;
                }

                CheckBox {
                    id: ssao_range_check;
                    text: "Range-Check"
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
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
                        id: csm_enabled;
                        x: menu_width - this.width - 30
                        y: -10
                        onCheckStateChanged: map.shared_config.csm_enabled = this.checked;
                    }
                }

                CheckBox {
                    id: overlay_shadowmaps;
                    text: "Overlay Shadow-Maps"
                    Layout.fillWidth: true;
                    Layout.columnSpan: 2;
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
                        text: "Overlays"
                        font.pixelSize:16
                    }
                }

                Label { text: "Pre-Shading:" }
                ComboBox {
                    id: debug_overlay;
                    currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
                    Layout.fillWidth: true;
                    model: ["None", "Ortho-Picture", "Normals", "Tiles", "Zoomlevel", "Vertex-ID", "Vertex Height-Sample", "SSAO Buffer"]
                    onCurrentIndexChanged:  map.shared_config.debug_overlay = currentIndex;
                }

                Label { text: "Strength:" }
                ValSlider {
                    id: debug_overlay_strength;
                    from: 0.0; to: 1.0; stepSize:  0.01;
                    onMoved: map.shared_config.debug_overlay_strength = value;
                }
            }
        }
    }

}

