layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out highp uint texout_depth;
layout (location = 4) out highp int texout_vertex_id;


flat in int vertex_id;

void main() {
    vec3 color = vec3(1.0, 0.0, 0.0);
    texout_vertex_id = vertex_id;
    texout_albedo = color;
}