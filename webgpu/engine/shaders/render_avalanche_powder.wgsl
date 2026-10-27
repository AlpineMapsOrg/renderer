/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

// The MLS-MPM particles as powder (AvalancheRenderer::draw_transparent): each particle a large,
// soft, transparent blob, dense in the middle and fading to nothing at its edge, blended over
// the lit frame after the compose pass (like the track lines). Meant as a light haze over the
// flow surface, which carries the mass; it shows where snow is in the air or thin.
// - Only the part of a blob in front of the scene counts: the alpha is scaled by the share of
//   the ray's path through the blob that lies before the G-buffer depth, so blobs sink softly
//   into the terrain and the flow surface instead of being cut off at them.
// - All blobs have the same colour, so "over" blending gives the same result in any order: no
//   sorting.
// - Each blob is cloudy, not a smooth disc: value noise across it, sampled at an offset of its
//   own (a hash of the particle index), so no two blobs look alike and each keeps its pattern
//   as it moves.
// - Lit like the terrain (ambient + sun, wrapped for a translucent look) and hazed by the
//   atmosphere, both per vertex: the fragments only shade the sphere's normal.

///use util/shared_config
///use util/camera_config
///use util/atmosphere
///use webgpu::tile_util

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var depth_texture: texture_2d<f32>;
@group(3) @binding(0) var<storage, read> particles: array<vec4f>;
@group(3) @binding(1) var<uniform> avalanche: AvalancheConfig;
@group(3) @binding(2) var<storage, read> previous: array<vec4f>; // see render_avalanche.wgsl

// Must match AvalancheRenderer::GpuConfig (and render_avalanche.wgsl).
struct AvalancheConfig {
    xy_from_metres: vec4f,
    offset_cws: vec2f,
    radius: f32, // [m] the blob radius
    colour_mode: u32, // 0 snow, 1 speed
    min_pixel_radius: f32,
    speed_max: f32, // [m/s]
    particle_count: u32,
    strain_window: f32,
    children: u32,
    opacity: f32, // peak alpha of a blob
    interpolation: f32, // share of the way from `previous` to `particles`, see render_avalanche.wgsl
    padding2: u32,
}

const PARTICLE_STRIDE_VEC4: u32 = 8u;
const SNOW_ALBEDO = vec3f(0.93, 0.95, 0.98);
const FALLOFF = 3.0; // Gaussian exp(-FALLOFF r^2) inside the blob, r in blob radii
const NOISE_SCALE = 2.5; // noise cells across a blob radius
const NOISE_STRENGTH = 0.85; // 0: smooth blobs, 1: alpha fully modulated by the noise

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) corner: vec2f, // [-1, 1] across the blob
    @location(1) @interpolate(flat) right: vec3f,
    @location(2) @interpolate(flat) up: vec3f,
    @location(3) @interpolate(flat) to_camera: vec3f,
    @location(4) @interpolate(flat) centre_depth: f32, // view depth of the centre, world units
    @location(5) @interpolate(flat) radius: f32, // world units
    @location(6) @interpolate(flat) albedo: vec3f,
    @location(7) @interpolate(flat) transmittance: vec3f, // atmosphere between camera and blob
    @location(8) @interpolate(flat) inscatter: vec3f,
    @location(9) @interpolate(flat) noise_offset: vec2f, // this blob's place in the noise
}

fn viridis(f: f32) -> vec3f {
    let stops = array<vec3f, 5>(vec3f(0.267, 0.005, 0.329), vec3f(0.230, 0.322, 0.546), vec3f(0.128, 0.567, 0.551),
        vec3f(0.369, 0.789, 0.383), vec3f(0.993, 0.906, 0.144));
    let s = clamp(f, 0.0, 1.0) * 4.0;
    let i = min(u32(s), 3u);
    return mix(stops[i], stops[i + 1u], s - f32(i));
}

// Integer hash (PCG-style), as in render_avalanche.wgsl.
fn hash_u32(x: u32) -> u32 {
    let v = x * 747796405u + 2891336453u;
    let w = ((v >> ((v >> 28u) + 4u)) ^ v) * 277803737u;
    return (w >> 22u) ^ w;
}

fn hash2(cell: vec2i) -> f32 {
    return f32(hash_u32((u32(cell.x) * 1664525u) ^ (u32(cell.y) * 1013904223u))) / 4294967296.0;
}

// Value noise in [0, 1], smoothly interpolated between hashed lattice values.
fn value_noise(p: vec2f) -> f32 {
    let cell = vec2i(floor(p));
    let f = p - floor(p);
    let u = f * f * (3.0 - 2.0 * f);
    let a = hash2(cell);
    let b = hash2(cell + vec2i(1, 0));
    let c = hash2(cell + vec2i(0, 1));
    let d = hash2(cell + vec2i(1, 1));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Three octaves: billows with finer wisps.
fn cloud_noise(p: vec2f) -> f32 {
    return 0.55 * value_noise(p) + 0.3 * value_noise(2.03 * p + 17.0) + 0.15 * value_noise(4.01 * p + 43.0);
}

fn hidden() -> VertexOut {
    var out: VertexOut;
    out.position = vec4f(2.0, 2.0, 2.0, 1.0); // outside the clip volume
    return out;
}

fn view_depth(pos_cws: vec3f) -> f32 {
    return -(camera.view_matrix * vec4f(pos_cws, 1.0)).z;
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance: u32) -> VertexOut {
    let base = instance * PARTICLE_STRIDE_VEC4;
    let current = particles[base];
    let before = previous[base]; // an empty slot before (entrained since): no blend, as in render_avalanche.wgsl
    let p0 = vec4f(mix(select(current.xyz, before.xyz, before.w > 0.0), current.xyz, avalanche.interpolation), current.w);
    if p0.w <= 0.0 || instance >= avalanche.particle_count {
        return hidden(); // inactive
    }
    let p1 = particles[base + 1u];

    // Metres to camera-relative world, as in render_avalanche.wgsl.
    let m = avalanche.xy_from_metres;
    let xy = vec2f(m.x * p0.x + m.z * p0.y, m.y * p0.x + m.w * p0.y) + avalanche.offset_cws;
    let world_per_metre = 1.0 / cos(y_to_lat(xy.y + camera.position.y));
    let centre = vec3f(xy, p0.z * world_per_metre - camera.position.z);
    let dist = length(centre);
    let pixel = 2.0 * dist / (camera.viewport_size.y * camera.proj_matrix[1][1]);
    let radius = max(avalanche.radius * world_per_metre, avalanche.min_pixel_radius * pixel);
    if dist <= radius * 1.01 {
        return hidden(); // camera inside the blob
    }

    let dir = centre / dist;
    let camera_up = camera.inv_view_matrix[1].xyz;
    var right = cross(dir, camera_up);
    right = select(cross(dir, vec3f(1.0, 0.0, 0.0)), right, dot(right, right) > 1e-8);
    right = normalize(right);
    let up = cross(right, dir);
    let corner = vec2f(f32(vertex_index & 1u) * 2.0 - 1.0, f32(vertex_index >> 1u) * 2.0 - 1.0);
    let pos = centre + (right * corner.x + up * corner.y) * radius;

    var out: VertexOut;
    out.position = camera.view_proj_matrix * vec4f(pos, 1.0);
    out.corner = corner;
    out.right = right;
    out.up = up;
    out.to_camera = -dir;
    out.centre_depth = view_depth(centre);
    out.radius = radius;
    out.albedo = SNOW_ALBEDO;
    if avalanche.colour_mode == 1u {
        out.albedo = viridis(length(p1.xyz) / avalanche.speed_max);
    }
    out.noise_offset = vec2f(f32(hash_u32(instance) & 0xffffu), f32(hash_u32(instance ^ 0x9e3779b9u) & 0xffffu)) * 0.37;
    out.transmittance = vec3f(1.0);
    out.inscatter = vec3f(0.0);
    if bool(config.atmosphere_enabled) {
        let origin = camera.position.xyz / 1000.0;
        out.inscatter = calculate_atmospheric_light(origin, dir, dist / 1000.0, vec3f(0.0), 10);
        out.transmittance = calculate_atmospheric_light(origin, dir, dist / 1000.0, vec3f(1.0), 10) - out.inscatter;
    }
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let r2 = dot(in.corner, in.corner);
    if r2 >= 1.0 {
        discard;
    }
    // The ray's path through the blob, in view depth, and the part of it before the scene.
    let half_path = in.radius * sqrt(1.0 - r2);
    let front = in.centre_depth - half_path;
    let scene_ndc = textureLoad(depth_texture, vec2u(in.position.xy), 0).x;
    var visible = 1.0;
    if scene_ndc > 0.0 { // reverse-Z: 0 is the sky
        let ndc = vec2f(2.0 * in.position.x / camera.viewport_size.x - 1.0, 1.0 - 2.0 * in.position.y / camera.viewport_size.y);
        let scene = camera.inv_proj_matrix * vec4f(ndc, scene_ndc, 1.0);
        let scene_depth = -scene.z / scene.w;
        visible = clamp((scene_depth - front) / (2.0 * half_path), 0.0, 1.0);
    }
    let cloud = clamp(1.8 * cloud_noise(in.corner * NOISE_SCALE + in.noise_offset) - 0.35, 0.0, 1.5);
    let alpha = avalanche.opacity * exp(-FALLOFF * r2) * (1.0 - r2) * visible * mix(1.0, cloud, NOISE_STRENGTH);
    if alpha <= 0.002 {
        discard;
    }

    // Sphere normal, wrapped diffuse lighting: soft, as light passes through loose snow.
    let normal = normalize(in.right * in.corner.x + in.up * in.corner.y + in.to_camera * sqrt(1.0 - r2));
    var colour = in.albedo;
    if bool(config.shading_enabled) {
        let to_light = -normalize(config.sun_light_dir.xyz);
        let wrapped = 0.5 + 0.5 * dot(normal, to_light);
        let material = config.material_light_response;
        colour = in.albedo * (material.r * config.amb_light.rgb * config.amb_light.a
                     + material.g * config.sun_light.rgb * config.sun_light.a * wrapped);
    }
    colour = colour * in.transmittance + in.inscatter;
    return vec4f(colour * alpha, alpha); // premultiplied
}
