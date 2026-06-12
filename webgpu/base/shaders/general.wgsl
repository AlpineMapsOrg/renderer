/*****************************************************************************
* weBIGeo
* Copyright (C) 2026 Gerald Kimmersdorfer
* Copyright (C) 2024 Patrick Komon
* Copyright (C) 2022 Adam Celarek
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http : //www.gnu.org/licenses/>.
*****************************************************************************/

//General-purpose, dependency-free helper functions shared across shaders.

//Linear falloff from 1.0 at `lower` to 0.0 at `upper`, clamped to [0, 1].
fn calculate_falloff(dist: f32, lower: f32, upper: f32) -> f32 {
    return clamp(1.0 - (dist - lower) / (upper - lower), 0.0, 1.0);
}

//Smoothed band: 1.0 inside [min, max], falling off over `smoothf` beyond each edge.
fn calculate_band_falloff(val: f32, min: f32, max: f32, smoothf: f32) -> f32 {
    if val < min {
        return calculate_falloff(val, min + smoothf, min);
    } else if val > max {
        return calculate_falloff(val, max, max + smoothf);
    } else {
        return 1.0;
    }
}
