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

// MLS-MPM avalanche particles as sphere impostors in the G-buffer (AvalancheRenderer).
// One instance per particle, four vertices: a quad facing the camera that covers the
// sphere's silhouette. The fragment shader intersects the view ray with the sphere and writes
// the hit's depth, camera-relative position, normal and albedo, like render_tiles does for
// the terrain - so the compose pass lights, hazes and shadows both the same way.

///use util/shared_config
///use util/camera_config
///use webgpu::encoder
///use webgpu::tile_util

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
// struct Particle of the solver (webgpu/compute/shaders/mpm_common.wgsl) as eight vec4s:
// [0] position (metres xy, altitude z), mass (0 = inactive); [1] velocity, plastic state.
@group(2) @binding(0) var<storage, read> particles: array<vec4f>;
@group(2) @binding(1) var<uniform> avalanche: AvalancheConfig;

const PARTICLE_STRIDE_VEC4: u32 = 8u;
const SNOW_ALBEDO = vec3f(0.93, 0.95, 0.98);

struct AvalancheConfig {
    xy_from_metres: vec4f, // 2x2 linear part, column-major: world.xy = (x, y) metres mapped
    offset_cws: vec2f, // world offset of metre (0, 0) minus the camera position
    radius: f32, // [m]
    colour_mode: u32, // 0 snow, 1 speed
    min_pixel_radius: f32,
    speed_max: f32, // [m/s]
    particle_count: u32,
    padding: f32,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) pos_cws: vec3f, // the quad point, camera-relative world
    @location(1) @interpolate(flat) centre_cws: vec3f,
    @location(2) @interpolate(flat) radius: f32, // world units
    @location(3) @interpolate(flat) colour: vec3f,
}

struct FragOut {
    @builtin(frag_depth) depth: f32,
    @location(0) albedo: u32,
    @location(1) position: vec4f,
    @location(2) normal_enc: vec2u,
    @location(3) overlay: u32,
}

// Viridis at five stops, as the sandbox colour scale.
fn viridis(f: f32) -> vec3f {
    let stops = array<vec3f, 5>(vec3f(0.267, 0.005, 0.329), vec3f(0.230, 0.322, 0.546), vec3f(0.128, 0.567, 0.551),
        vec3f(0.369, 0.789, 0.383), vec3f(0.993, 0.906, 0.144));
    let s = clamp(f, 0.0, 1.0) * 4.0;
    let i = min(u32(s), 3u);
    return mix(stops[i], stops[i + 1u], s - f32(i));
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance: u32) -> VertexOut {
    var out: VertexOut;
    let p0 = particles[instance * PARTICLE_STRIDE_VEC4];
    let p1 = particles[instance * PARTICLE_STRIDE_VEC4 + 1u];
    if p0.w <= 0.0 || instance >= avalanche.particle_count {
        out.position = vec4f(2.0, 2.0, 2.0, 1.0); // inactive: outside the clip volume
        return out;
    }

    // Metres to camera-relative world. Heights are real metres, world units are Web Mercator,
    // so z is stretched by 1/cos(latitude) like the terrain (render_tiles).
    let m = avalanche.xy_from_metres;
    let xy = vec2f(m.x * p0.x + m.z * p0.y, m.y * p0.x + m.w * p0.y) + avalanche.offset_cws;
    let world_per_metre = 1.0 / cos(y_to_lat(xy.y + camera.position.y));
    let centre = vec3f(xy, p0.z * world_per_metre - camera.position.z);

    // Radius in world units, at least min_pixel_radius pixels so the flow stays visible from afar.
    let dist = length(centre);
    let pixel = 2.0 * dist / (camera.viewport_size.y * camera.proj_matrix[1][1]);
    let radius = max(avalanche.radius * world_per_metre, avalanche.min_pixel_radius * pixel);
    if dist <= radius * 1.01 {
        out.position = vec4f(2.0, 2.0, 2.0, 1.0); // camera inside the sphere
        return out;
    }

    // A quad through the centre, perpendicular to the view ray, just large enough for the
    // silhouette (which grows past the radius as the sphere comes close).
    let dir = centre / dist;
    let camera_up = camera.inv_view_matrix[1].xyz;
    var right = cross(dir, camera_up);
    right = select(cross(dir, vec3f(1.0, 0.0, 0.0)), right, dot(right, right) > 1e-8);
    right = normalize(right);
    let up = cross(right, dir);
    let half_size = radius * dist / sqrt(dist * dist - radius * radius);
    let corner = vec2f(f32(vertex_index & 1u) * 2.0 - 1.0, f32(vertex_index >> 1u) * 2.0 - 1.0);
    let pos = centre + (right * corner.x + up * corner.y) * half_size;

    out.position = camera.view_proj_matrix * vec4f(pos, 1.0);
    out.pos_cws = pos;
    out.centre_cws = centre;
    out.radius = radius;
    if avalanche.colour_mode == 1u {
        out.colour = viridis(length(p1.xyz) / avalanche.speed_max);
    } else {
        out.colour = SNOW_ALBEDO;
    }
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> FragOut {
    // Ray from the camera (the origin) through this fragment against the sphere. The distance
    // from the centre to the ray is formed directly (q), not as a difference of squared
    // lengths, which would cancel catastrophically at a few km for a metre-sized sphere.
    let dir = normalize(in.pos_cws);
    let along = dot(dir, in.centre_cws);
    let q = in.centre_cws - dir * along;
    let h2 = in.radius * in.radius - dot(q, q);
    if h2 < 0.0 || along <= 0.0 {
        discard;
    }
    let hit = dir * (along - sqrt(h2));
    let normal = normalize(hit - in.centre_cws);
    let clip = camera.view_proj_matrix * vec4f(hit, 1.0);

    var out: FragOut;
    out.depth = clip.z / clip.w;
    out.albedo = pack4x8unorm(vec4f(in.colour, 1.0));
    out.position = vec4f(hit, length(hit));
    out.normal_enc = octNormalEncode2u16(normal);
    out.overlay = 0u;
    return out;
}
