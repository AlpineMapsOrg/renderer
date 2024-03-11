
struct camera_config {
    position: vec4f,
    view_matrix: mat4x4f,
    proj_matrix: mat4x4f,
    view_proj_matrix: mat4x4f,
    inv_view_proj_matrix: mat4x4f,
    inv_view_matrix: mat4x4f,
    inv_proj_matrix: mat4x4f,
    viewport_size: vec2f,
    distance_scaling_factor: f32,
    buffer2: f32,
};

// Converts the given world coordinates (relative to camera) to the
// normalised device coordinates (range [0,1])
fn ws_to_ndc(pos_ws: vec3f) -> vec3f {
    var camera: camera_config; //TODO remove
    var tmp = camera.view_proj_matrix * vec4(pos_ws, 1.0); // from ws to clip-space

    // perspective divide
    tmp.x /= tmp.w;
    tmp.y /= tmp.w;
    tmp.z /= tmp.w;

    return tmp.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
}

fn depth_cs_to_pos_ws(depth: f32,  tex_coords: vec2f) -> vec3f {
    var camera: camera_config; //TODO remove
    var clip_space_position = vec4f(tex_coords * 2.0 - vec2(1.0), 2.0 * depth - 1.0, 1.0);
    var position = camera.inv_view_proj_matrix * clip_space_position; // Use this for world space
    return (position.xyz / position.w);
}

fn depth_cs_to_pos_vs(depth: f32, tex_coords: vec2f) -> vec3f {
    var camera: camera_config;
    var clip_space_position = vec4f(tex_coords * 2.0 - vec2(1.0), 2.0 * depth - 1.0, 1.0);
    var position = camera.inv_proj_matrix * clip_space_position; // Use this for view space
    return (position.xyz / position.w);
}
