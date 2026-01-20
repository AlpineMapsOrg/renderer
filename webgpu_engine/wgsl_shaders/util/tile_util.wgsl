/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

const PI: f32 = 3.1415926535897932384626433;
const SEMI_MAJOR_AXIS: f32 = 6378137;
const EARTH_CIRCUMFERENCE: f32 = 2 * PI * SEMI_MAJOR_AXIS;
const ORIGIN_SHIFT: f32 = PI * SEMI_MAJOR_AXIS;

struct TileId {
    x: u32,
    y: u32,
    zoomlevel: u32,
    alignment: u32,
}

const EMPTY_TILE_ZOOMLEVEL: u32 = 4294967295u;

fn tile_ids_equal(a: TileId, b: TileId) -> bool { return a.x == b.x && a.y == b.y && a.zoomlevel == b.zoomlevel; }
fn tile_id_empty(id: TileId) -> bool { return id.zoomlevel == EMPTY_TILE_ZOOMLEVEL; }

struct Bounds {
    min: vec2f,
    max: vec2f,
}

fn y_to_lat(y: f32) -> f32 {
    let mercN = y * PI / ORIGIN_SHIFT;
    let latRad = 2.f * (atan(exp(mercN)) - (PI / 4.0));
    return latRad;
}

fn calc_altitude_correction_factor(y: f32) -> f32 { return 0.125 / cos(y_to_lat(y)); }

// equivalent of nucleus::srs::number_of_horizontal_tiles_for_zoom_level
fn number_of_horizontal_tiles_for_zoom_level(z: u32) -> u32 { return u32(1 << z); }

// equivalent of nucleus::srs::number_of_vertical_tiles_for_zoom_level
fn number_of_vertical_tiles_for_zoom_level(z: u32) -> u32 { return u32(1 << z); }

// equivalent of nucleus::srs::tile_bounds(tile::Id)
fn calculate_bounds(tile_id: TileId) -> vec4<f32> {
    const absolute_min = vec2f(-ORIGIN_SHIFT, -ORIGIN_SHIFT);
    let width_of_a_tile: f32 = EARTH_CIRCUMFERENCE / f32(number_of_horizontal_tiles_for_zoom_level(tile_id.zoomlevel));
    let height_of_a_tile: f32 = EARTH_CIRCUMFERENCE / f32(number_of_vertical_tiles_for_zoom_level(tile_id.zoomlevel));
    let min = absolute_min + vec2f(f32(tile_id.x) * width_of_a_tile, f32(tile_id.y) * height_of_a_tile);
    let max = min + vec2f(width_of_a_tile, height_of_a_tile);
    return vec4f(min.x, min.y, max.x, max.y);
}

fn world_to_lat_long_alt(pos_ws: vec3f) -> vec3f {
    let mercN = pos_ws.y * PI / ORIGIN_SHIFT;
    let latRad = 2.0 * (atan(exp(mercN)) - (PI / 4.0));
    let latitude = latRad * 180.0 / PI;
    let longitude = (pos_ws.x + ORIGIN_SHIFT) / (ORIGIN_SHIFT / 180.0) - 180.0;
    let altitude = pos_ws.z * cos(latitude * PI / 180.0);
    return vec3f(latitude, longitude, altitude);
}


// ported from alpine maps
fn decrease_zoom_level_by_one(
    input_tile_id: TileId,
    input_uv: vec2f,
    output_tile_id: ptr<function, TileId>,
    output_uv: ptr<function, vec2f>,
) -> bool {
    if (input_tile_id.zoomlevel == 0u) {
        return false;
    }

    let x_border = f32(input_tile_id.x & 1u) / 2.0;
    let y_border = f32((input_tile_id.y & 1u) == 0u) / 2.0;
    
    *output_tile_id = TileId( input_tile_id.x / 2u, input_tile_id.y / 2u, input_tile_id.zoomlevel - 1u, 0);
    *output_uv = input_uv / 2.0 + vec2f(x_border, y_border);

    return true;
}

// ported from alpine maps
fn decrease_zoom_level_until(
    input_tile_id: TileId,
    input_uv: vec2f,
    zoomlevel: u32,
    output_tile_id: ptr<function, TileId>,
    output_uv: ptr<function, vec2f>,
) -> bool {
    if (input_tile_id.zoomlevel <= zoomlevel) {
        *output_tile_id = input_tile_id;
        *output_uv = input_uv;
        return false;
    }

    let z_delta: u32 = input_tile_id.zoomlevel - zoomlevel;
    let border_mask: u32 = (1u << z_delta) - 1u;
    let x_border = f32(input_tile_id.x & border_mask) / f32(1u << z_delta);
    let y_border = f32((input_tile_id.y ^ border_mask) & border_mask) / f32(1u << z_delta);

    *output_tile_id = TileId(
        input_tile_id.x >> z_delta,
        input_tile_id.y >> z_delta,
        input_tile_id.zoomlevel - z_delta,
        0);
    *output_uv = input_uv / f32(1u << z_delta) + vec2f(x_border, y_border);

    return true;
}


fn increase_zoom_level_by_one(
    input_tile_id: TileId,
    input_uv: vec2f,
    output_tile_id: ptr<function, TileId>,
    output_uv: ptr<function, vec2f>,
) -> bool {
    if (input_tile_id.zoomlevel == 18u) {
        return false;
    }

    let x_border = select(0u, 1u, input_uv.x >= 0.5);
    let y_border = select(0u, 1u, input_uv.y >= 0.5);

    var higher_zoomlevel_tile_id: TileId;
    higher_zoomlevel_tile_id.x = 2u * input_tile_id.x + x_border;
    higher_zoomlevel_tile_id.y = 2u * input_tile_id.y - y_border + 1;
    higher_zoomlevel_tile_id.zoomlevel = input_tile_id.zoomlevel + 1u;
    *output_tile_id = higher_zoomlevel_tile_id;
    *output_uv = 2.0 * input_uv - vec2f(f32(x_border), f32(y_border));
    return true;
}

//TODO optimize similarly to decrease_zoom_level_until
fn increase_zoom_level_until(
    input_tile_id: TileId,
    input_uv: vec2f,
    zoomlevel: u32,
    output_tile_id: ptr<function, TileId>,
    output_uv: ptr<function, vec2f>,
) -> bool {
    if (input_tile_id.zoomlevel >= zoomlevel) {
        return false;
    }


    var output_zoomlevel = input_tile_id.zoomlevel;
    while (output_zoomlevel < zoomlevel) {
        increase_zoom_level_by_one(input_tile_id, input_uv, output_tile_id, output_uv);
        output_zoomlevel++;
    }
    return true;
}

fn calc_tile_id_and_uv_for_zoom_level(
    input_tile_id: TileId,
    input_uv: vec2f,
    zoomlevel: u32,
    output_tile_id: ptr<function, TileId>,
    output_uv: ptr<function, vec2f>,
) {
    if (input_tile_id.zoomlevel == zoomlevel) {
        *output_tile_id = input_tile_id;
        *output_uv = input_uv;
    } else if (input_tile_id.zoomlevel < zoomlevel) {
        increase_zoom_level_until(input_tile_id, input_uv, zoomlevel, output_tile_id, output_uv);
    } else {
        decrease_zoom_level_until(input_tile_id, input_uv, zoomlevel, output_tile_id, output_uv);
    }
}