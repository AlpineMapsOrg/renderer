/*****************************************************************************
 * Alpine Terrain Renderer
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
import Alpine

import "components"

SetPanel {

    function update_control_values() {
        let conf = map.shared_config;
        wireframe_mode.currentIndex = conf.wireframe_mode;
        normal_mode.currentIndex = conf.normal_mode;
        curtain_settings_mode.currentIndex = conf.curtain_settings.x;
        curtain_settings_height_mode.currentIndex = conf.curtain_settings.y;
        curtain_settings_height_reference.value = conf.curtain_settings.z;
        height_lines_enabled.checked = conf.height_lines_enabled;
        phong_enabled.checked = conf.phong_enabled;
        sun_light_color.color = Qt.rgba(conf.sun_light.x, conf.sun_light.y, conf.sun_light.z, conf.sun_light.w);
        amb_light_color.color = Qt.rgba(conf.amb_light.x, conf.amb_light.y, conf.amb_light.z, conf.amb_light.w);
        material_color.color = Qt.rgba(conf.material_color.x, conf.material_color.y, conf.material_color.z, conf.material_color.w);
        material_light_response.vector = conf.material_light_response;
        ssao_enabled.checked = conf.ssao_enabled;
        ssao_kernel.value = conf.ssao_kernel;
        ssao_falloff_to_value.value = conf.ssao_falloff_to_value;
        ssao_blur_kernel_size.value = conf.ssao_blur_kernel_size;
        ssao_range_check.checked = conf.ssao_range_check;
        csm_enabled.checked = conf.csm_enabled;
        overlay_shadowmaps.checked = conf.overlay_shadowmaps_enabled;
        overlay_mode.currentIndex = overlay_mode.indexOfValue(conf.overlay_mode);
        overlay_strength.value = conf.overlay_strength;
        overlay_postshading_enabled.checked = conf.overlay_postshading_enabled;
    }

    Component.onCompleted: update_control_values()

    Item {  // i need to pack Connections in an item (dunno why)
        Connections {
            target: map
            function onHud_visible_changed(hud_visible) {
                //debugMenu.visible = hud_visible;
            }
            function onShared_config_changed(conf) {
                update_control_values();
            }
        }
    }

    CheckGroup {

        Label { text: "Overlay:" }
        ComboBox {
            id: overlay_mode;
            textRole: "text"
            valueRole: "value"
            currentIndex: 0; // Init with 0 necessary otherwise onCurrentIndexChanged gets emited on startup (because def:-1)!
            Layout.fillWidth: true;
            // NOTE: All overlay with values >= 100 are evaluated in the compose step and therefore
            // can be applied pre- and post-shading. The other ones are applied in the gbuffer step
            // and are therefore applied on the albedo map. Post-Shading is not possible!!
            model: [
                { text: "None",                 value: 0    },
                { text: "Normals",              value: 1    }, // (only in tile.frag)
                { text: "Tiles",                value: 2    }, // (only in tile.frag)
                { text: "Zoomlevel",            value: 3    }, // (only in tile.frag)
                { text: "Vertex-ID",            value: 4    }, // (only in tile.frag)
                { text: "Vertex Height-Sample", value: 5    }, // (only in tile.frag)
                { text: "Decoded Normals",      value: 100  },
                { text: "Steepness",            value: 101  },
                { text: "SSAO Buffer",          value: 102  },
                { text: "Shadow Cascades",      value: 103  }
            ]
            onActivated:  map.shared_config.overlay_mode = currentValue;
        }

        Label {
            text: "Strength:"
            visible: overlay_strength.visible;
        }
        ValSlider {
            id: overlay_strength;
            from: 0.0; to: 1.0; stepSize:  0.01;
            visible: overlay_mode.currentValue > 0
            onMoved: map.shared_config.overlay_strength = value;
        }

        CheckBox {
            id: overlay_postshading_enabled;
            text: "overlay post shading"
            visible: overlay_mode.currentValue >= 100
            Layout.fillWidth: true;
            Layout.columnSpan: 2;
            onCheckStateChanged: map.shared_config.overlay_postshading_enabled = this.checked;
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

    }

    CheckGroup {
        name: "Curtains"
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
    }

    CheckGroup {
        name: "Height-Lines"
        id: height_lines_enabled
        checkBoxEnabled: true
        onCheckedChanged: map.shared_config.height_lines_enabled = this.checked;
    }

    CheckGroup {
        name: "Shading"
        id: phong_enabled
        checkBoxEnabled: true
        onCheckedChanged: map.shared_config.phong_enabled = this.checked;

        Label { text: "Dir.-Light:" }
        ColorPicker {
            id: sun_light_color;
            onColorChanged: map.shared_config.sun_light = Qt.vector4d(color.r, color.g, color.b, color.a);
        }

        Label { text: "Amb.-Light:" }
        ColorPicker {
            id: amb_light_color;
            onColorChanged: map.shared_config.amb_light = Qt.vector4d(color.r, color.g, color.b, color.a);
        }

        Label { text: "Light-Direction:" }
        VectorEditor {
            vector: map.sun_angles;
            onVectorChanged: map.sun_angles = vector;
            dialogTitle: "Sun Light Direction";
            elementNames: ["Azimuth", "Zenith"];
            elementFroms: [0.0, -180.0];
            elementTos: [360.0, 180.0];
            dim: false;
            enabled: !map.settings.gl_sundir_date_link;
        }

        Label { text: "Mat.-Color:" }
        ColorPicker {
            id: material_color;
            onColorChanged: map.shared_config.material_color = Qt.vector4d(color.r, color.g, color.b, color.a);
        }

        Label { text: "Light-Response:" }
        VectorEditor {
            id: material_light_response;
            vector: map.shared_config.material_light_response;
            onVectorChanged: map.shared_config.material_light_response = vector;
            dialogTitle: "Material Light-Response";
            elementNames: ["Ambient", "Diffuse", "Specular", "Shininess"];
            elementFroms: [0.0, 0.0, 0.0, 0.0]
            elementTos: [5.0, 5.0, 5.0, 128.0]
            dim: false;
        }

    }

    CheckGroup {
        id: ssao_enabled
        name: "Ambient Occlusion"
        checkBoxEnabled: true
        onCheckedChanged: map.shared_config.ssao_enabled = this.checked;

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

    }

    CheckGroup {
        id: csm_enabled
        checkBoxEnabled: true
        onCheckedChanged: map.shared_config.csm_enabled = this.checked;
        name: "Shadow Mapping"

        CheckBox {
            id: overlay_shadowmaps;
            text: "Overlay Shadow-Maps"
            Layout.fillWidth: true;
            Layout.columnSpan: 2;
            onCheckStateChanged: map.shared_config.overlay_shadowmaps = this.checked;
        }
    }

}
