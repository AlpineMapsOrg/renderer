layout(binding = 0) uniform sampler2D texture_sampler;
layout(binding = 1) uniform sampler2D shadow_map_sampler;

uniform int out_mode;

in lowp vec2 uv;
in highp vec4 shadow_coords;

out lowp vec4 out_Color;

void main() {
/*
   highp float dist = pos_to_cam.z;

   out_Color = vec4(lowp vec3(1.f, 0.f, 0.f) * smoothstep(50, 1200, dist), 1.f);
*/

    if (out_mode == 0) {
        highp vec4 factor;

       if (texture(shadow_map_sampler, shadow_coords.xy).x < shadow_coords.z - 0.005) {
            out_Color = vec4(0.0f, 0.0f, 1.f, 1.f);
       } else {
            highp vec4 ortho = texture(texture_sampler, uv);
            out_Color =  vec4(vec3(ortho), 1.0);
       }
    } else if (out_mode == 1) {
        highp vec4 ortho = texture(texture_sampler, uv);

        out_Color = vec4(vec3(ortho), 1.f);
    }




   // out_Color = vec4(shadow_coords.xy, texture(shadow_map_sampler, shadow_coords.xy).x - shadow_coords.z, 1.f);
    /*
    highp vec4 ortho = texture(texture_sampler, uv);
    out_Color =  ;
    */
/*
    highp vec4 ortho = texture(texture_sampler, uv);

    if (shadow_coords.x > 0 && shadow_coords.x < 1 &&
        shadow_coords.y > 0 && shadow_coords.y < 1)
    {
        out_Color = vec4(vec3(ortho), 1.0) * vec4(0.5f, 1.f, 0.5f, 1.f);
    } else {
        out_Color = vec4(vec3(ortho), 1.0) * vec4(1.f, 0.5f, 0.5f, 1.f);
    }
*/

  // out_Color = vec4(texture(shadow_map_sampler, shadow_coords.xy).xyz, 1.f);
//   highp float normalized = shadow_coords.z / shadow_coords.w;
//   highp float normal = shadow_coords.z;
//   out_Color = vec4(normal, normal, normal, 1.f);


}
