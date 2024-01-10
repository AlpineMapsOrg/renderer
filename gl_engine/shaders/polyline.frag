layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out highp uint texout_depth;
layout (location = 4) out highp uint texout_vertex_id;

in highp vec3 color;

flat in highp int vertex_id;

void main() {
    texout_vertex_id = uint(vertex_id);
    //texout_albedo = color;
    // intersect here?
    // attenuation?
    // specular hightlight?
}