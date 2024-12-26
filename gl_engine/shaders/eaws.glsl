/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Joerg Christian Reiher
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

layout (std140) uniform eaws_reports {
    // length of array must be the same as in avalanche::eaws::uboEawsReports on host side
    uvec4 reports[1000];
} eaws;

vec3 color_from_eaws_danger_rating(uint rating)
{
    if(0u == rating) return vec3(0.f,1.f,0.f);      // green        for 0 = low
    if(1u == rating) return vec3(1.f,1.f,0.f);      // yellow       for 1 = moderate
    if(2u == rating) return vec3(1.f,0.53f,0.f);    // orange       for 2 = considerable
    if(3u == rating) return vec3(1.f,0.f,0.f);      // red          for 3 = high
    if(4u == rating) return vec3(0.5f,1.f,0.f);     // dark red     for 4 = extreme
    return(vec3(0.f,0.f,0.f));                      // white        for     undefined cases

}


