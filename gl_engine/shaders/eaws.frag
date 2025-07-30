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

#include "shared_config.glsl"
#include "camera_config.glsl"
#include "encoder.glsl"
#include "tile_id.glsl"
#include "eaws.glsl"

uniform highp usampler2DArray texture_sampler;
uniform highp usampler2D instanced_texture_array_index_sampler;
uniform highp usampler2D instanced_texture_zoom_sampler;

layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out lowp vec4 texout_depth;
layout (location = 4) out lowp vec3 texout_eaws;

flat in highp uvec3 var_tile_id;
in highp vec2 var_uv;
in highp vec3 var_pos_cws;
in highp vec3 var_normal;
flat in highp uint instance_id;


void main() {
    highp uvec3 tile_id = var_tile_id;
    highp vec2 uv = var_uv;

    decrease_zoom_level_until(tile_id, uv, texelFetch(instanced_texture_zoom_sampler, ivec2(instance_id, 0), 0).x);
    highp float texture_layer_f = float(texelFetch(instanced_texture_array_index_sampler, ivec2(instance_id, 0), 0).x);


    highp uint eawsRegionId = texelFetch(texture_sampler, ivec3(uv * 512, texture_layer_f),0).r;
    ivec4 report = eaws.reports[eawsRegionId];

    // // debug output regions
    //texout_eaws = vec3(float((eawsRegionId >> 8u) & 255u) / 256.0, float(eawsRegionId & 255u) / 256.0, float((eawsRegionId >> 16u) & 255u) / 256.0);
    //return;

    // Get altitude and slope normal
    float frag_height = var_pos_cws.z + camera.position.z;
    vec3 fragNormal = var_normal; // just to clarify naming

    // calculate frag color according to selected overlay type
    if(bool(conf.eaws_slope_angle_enabled)) // Slope Angle overlay is activated (does not require avalanche report)
    {
        // assign a color to slope angle obtained from (not normalized) normal
        texout_eaws = slopeAngleColorFromNormal(fragNormal);
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
            float margin = 25.f;           // margin within which colorblending between hi and lo happens
            if(frag_height > float(bound) + margin)
                texout_eaws =  color_from_eaws_danger_rating(ratingHi);
            else if (frag_height < float(bound) - margin)
                texout_eaws =  color_from_eaws_danger_rating(ratingLo);
            else
            {
                // around border: blend colors between upper and lower danger rating
                float a = (frag_height - (float(bound) - margin)) / (2*margin); // This is a value between 0 and 1
                texout_eaws = mix(color_from_eaws_danger_rating(ratingLo), color_from_eaws_danger_rating(ratingHi), a);
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
                texout_eaws =  color_from_snowCard_risk_parameters(ratingHi, fragNormal, unfavorable);
            else if (frag_height < float(bound) - margin)
                texout_eaws =  color_from_snowCard_risk_parameters(ratingLo, fragNormal, unfavorable);
            else
            {
                // around border: blend colors between upper and lower danger rating
                float a = (frag_height - (float(bound) - margin)) / (2*margin); // This is a value between 0 and 1
                vec3 colorLo = color_from_snowCard_risk_parameters(ratingLo, fragNormal, unfavorable);
                vec3 colorHi = color_from_snowCard_risk_parameters(ratingHi, fragNormal, unfavorable);
                texout_eaws = mix(colorLo, colorHi, a); // color_from_snowCard_risk_parameters(int eaws_danger_rating, int slope_angle_in_deg, bool unfavorable)
            }
        }

        //if Stop or GO Layer activated
        else if(bool(conf.eaws_stop_or_go_enabled))
        {
            // Get eaws danger rating from fragment altitude
            int eaws_danger_rating = frag_height >= float(bound)? ratingHi: ratingLo;
            texout_eaws = color_from_stop_or_go(fragNormal, eaws_danger_rating);
        }
    }
    else {
        // This color is defined in eaws.glsl (grey)
        texout_eaws = color_no_report_available;
    }
}
