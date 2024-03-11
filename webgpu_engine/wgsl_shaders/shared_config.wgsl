struct shared_config {
    sun_light: vec4f,
    sun_light_dir: vec4f,
    amb_light: vec4f,
    material_color: vec4f,
    material_light_response: vec4f,
    snow_settings_angle: vec4f,
    snow_settings_alt: vec4f,

    overlay_strength: f32,
    ssao_falloff_to_value: f32,
    padf1: f32,
    padf2: f32,

    phong_enabled: u32,
    normal_mode: u32,
    overlay_mode: u32,
    overlay_postshading_enabled: u32,

    ssao_enabled: u32,
    ssao_kernel: u32,
    ssao_range_check: u32,
    ssao_blur_kernel_size: u32,

    height_lines_enabled: u32,
    csm_enabled: u32,
    overlay_shadowmaps_enabled: u32,
    padi1: u32,
};