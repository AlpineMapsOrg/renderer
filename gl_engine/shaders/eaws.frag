/*****************************************************************************
* AlpineMaps.org
* Copyright (C) 2022 Adam Celarek
* Copyright (C) 2023 Gerald Kimmersdorfer
* Copyright (C) 2024 Jörg Christian Reiher
* Copyright (C) 2024 Johannes Eschner
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
#ifdef GL_ES
precision highp float;
#endif

#include "shared_config.glsl"
#include "camera_config.glsl"
#include "encoder.glsl"
#include "tile_id.glsl"
#include "eaws.glsl"

uniform highp usampler2DArray texture_sampler;
uniform highp usampler2D instanced_texture_array_index_sampler;
uniform highp usampler2D instanced_texture_zoom_sampler;
uniform highp sampler2DArray texture_sampler2;
uniform highp usampler2D instanced_texture_array_index_sampler2;
uniform highp usampler2D instanced_texture_zoom_sampler2;

layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out lowp vec4 texout_depth;

flat in highp uvec3 var_tile_id;
in highp vec2 var_uv;
in highp vec3 var_pos_cws;
in highp vec3 var_normal;
flat in highp uint instance_id;

#if CURTAIN_DEBUG_MODE > 0
in lowp float is_curtain;
#endif
flat in lowp vec3 vertex_color;

highp vec3 normal_by_fragment_position_interpolation() {
    highp vec3 dFdxPos = dFdx(var_pos_cws);
    highp vec3 dFdyPos = dFdy(var_pos_cws);
    return normalize(cross(dFdxPos, dFdyPos));
}


void main() {
#if CURTAIN_DEBUG_MODE == 2
    if (is_curtain == 0.0) {
        discard;
    }
#endif
    highp uvec3 tile_id = var_tile_id;
    highp vec2 uv = var_uv;

    // photo texture drawing
    decrease_zoom_level_until(tile_id, uv, texelFetch(instanced_texture_zoom_sampler2, ivec2(instance_id, 0), 0).x);
    highp float texture_layer2_f = float(texelFetch(instanced_texture_array_index_sampler2, ivec2(instance_id, 0), 0).x);

    lowp vec3 terrain_color = texture(texture_sampler2, vec3(uv, texture_layer2_f)).rgb;
    terrain_color = mix(terrain_color, conf.material_color.rgb, conf.material_color.a);

    // Write Position (and distance) in gbuffer
    highp float dist = length(var_pos_cws);
    texout_position = vec4(var_pos_cws, dist);

    // Write and encode normal in gbuffer
    highp vec3 normal = vec3(0.0);
    if (conf.normal_mode == 0u) normal = normal_by_fragment_position_interpolation();
    else normal = var_normal;
    texout_normal = octNormalEncode2u16(normal);

    // Write and encode distance for readback
    texout_depth = vec4(depthWSEncode2n8(dist), 0.0, 0.0);

    // HANDLE OVERLAYS (and mix it with the albedo color) THAT CAN JUST BE DONE IN THIS STAGE
    // (because of DATA thats not forwarded)
    // NOTE: Performancewise its generally better to handle overlays in the compose step! (screenspace effect)
    if (conf.overlay_mode > 0u && conf.overlay_mode < 100u) {
        lowp vec3 overlay_color = vec3(0.0);
        switch(conf.overlay_mode) {
            case 1u: overlay_color = normal * 0.5 + 0.5; break;
            default: overlay_color = vertex_color;
        }
        terrain_color = mix(terrain_color, overlay_color, conf.overlay_strength);
    }

#if CURTAIN_DEBUG_MODE == 1
    if (is_curtain > 0.0) {
        texout_albedo = vec3(1.0, 0.0, 0.0);
        return;
    }
#endif


    //From here on:  EAWS Layer Drawing
    decrease_zoom_level_until(tile_id, uv, texelFetch(instanced_texture_zoom_sampler, ivec2(instance_id, 0), 0).x);
    highp float texture_layer_f = float(texelFetch(instanced_texture_array_index_sampler, ivec2(instance_id, 0), 0).x);

    highp uint eawsRegionId = texelFetch(texture_sampler, ivec3(int(uv.x * float(512)), int(uv.y * float(512)) , texture_layer_f), 0).r;
    ivec4 report = eaws.reports[eawsRegionId];
    vec3 eaws_color;

    // // debug output regions
    //eaws_color = vec3(float((eawsRegionId >> 8u) & 255u) / 256.0, float(eawsRegionId & 255u) / 256.0, float((eawsRegionId >> 16u) & 255u) / 256.0);
    //return;

    // Get altitude and slope normal
    float frag_height = var_pos_cws.z + camera.position.z;
    vec3 fragNormal = var_normal; // just to clarify naming

    // calculate frag color according to selected overlay type
    if(bool(conf.eaws_slope_angle_enabled)) // Slope Angle overlay is activated (does not require avalanche report)
    {
        // assign a color to slope angle obtained from (not normalized) normal
        eaws_color = slopeAngleColorFromNormal(fragNormal);
    }
    else if(report.x >= 0) // avalanche report is available. report.x = -1 would mean no report available since .x stores the exposition as described in the  Masters THesis of Joey which must be >=0
    {
        // get avalanche ratings for eaws region of current fragment
        int bound = report.y;      // bound dividing moutain in Hi region and low region
        int ratingHi = report.a;   // rating should be value in {0,1,2,3,4}
        int ratingLo = report.z;   // rating should be value in {0,1,2,3,4}
        int rating = ratingLo;

        // if eaws report overlay activated: calculate color for danger level(blend at borders)
        if(bool(conf.eaws_danger_rating_enabled))
        {
            // color fragment according to danger level
            float margin = 25.0;           // margin within which colorblending between hi and lo happens
            if(frag_height > float(bound) + margin)
                eaws_color =  color_from_eaws_danger_rating(ratingHi);
            else if (frag_height < float(bound) - margin)
                eaws_color =  color_from_eaws_danger_rating(ratingLo);
            else
            {
                // around border: blend colors between upper and lower danger rating
                float a = (frag_height - (float(bound) - margin)) / (2.0*margin); // This is a value between 0 and 1
                eaws_color = mix(color_from_eaws_danger_rating(ratingLo), color_from_eaws_danger_rating(ratingHi), a);
            }
        }

        // If risk Level Overlay is activated, read unfavorable expositions information and check if fragment has unfavorable exposition
        else if(bool(conf.eaws_risk_level_enabled))
        {
            // report.x encodes dangerous directions bitwise as 1000000000 = North is unfavorable, 01000000 = NE is unfavorable etc.
            // direction() returns the direction of the fragment
            // the bitwise & comparison checks if direction bit is marked in report.x as unfavorable direction
            bool unfavorable = (0 != (report.x & direction(fragNormal)));

            // color the fragment according to danger level
            float margin = 25.f; // margin within which colorblending between hi and lo happens
            if(frag_height > float(bound) + margin)
                eaws_color =  color_from_snowCard_risk_parameters(ratingHi, fragNormal, unfavorable);
            else if (frag_height < float(bound) - margin)
                eaws_color =  color_from_snowCard_risk_parameters(ratingLo, fragNormal, unfavorable);
            else
            {
                // around border: blend colors between upper and lower danger rating
                float a = (frag_height - (float(bound) - margin)) / (2.0*margin); // This is a value between 0 and 1
                vec3 colorLo = color_from_snowCard_risk_parameters(ratingLo, fragNormal, unfavorable);
                vec3 colorHi = color_from_snowCard_risk_parameters(ratingHi, fragNormal, unfavorable);
                eaws_color = mix(colorLo, colorHi, a); // color_from_snowCard_risk_parameters(int eaws_danger_rating, int slope_angle_in_deg, bool unfavorable)
            }
        }

        //if Stop or GO Layer activated
        else if(bool(conf.eaws_stop_or_go_enabled))
        {
            // Get eaws danger rating from fragment altitude
            int eaws_danger_rating = frag_height >= float(bound)? ratingHi: ratingLo;
            eaws_color = color_from_stop_or_go(fragNormal, eaws_danger_rating);
        }
    }
    else {
        // This color is defined in eaws.glsl (grey)
        eaws_color = color_no_report_available;
    }

    // merge photo texture with eaws color
     if(eaws_color.r > 0.0 || eaws_color.g > 0.0 || eaws_color.b > 0.0)
         texout_albedo = mix(terrain_color, eaws_color, 0.5);
     else
        texout_albedo = terrain_color;
}
