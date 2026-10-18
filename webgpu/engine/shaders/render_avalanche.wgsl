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

// MLS-MPM avalanche particles as impostors in the G-buffer (AvalancheRenderer).
// One instance per particle, four vertices: a quad facing the camera that covers the
// particle's silhouette. The fragment shader intersects the view ray with a sphere, or with
// an ellipsoid stretched by the particle's strain rate, and writes the hit's depth,
// camera-relative position, normal and albedo, like render_tiles does for the terrain - so
// the compose pass lights, hazes and shadows both the same way.

///use util/shared_config
///use util/camera_config
///use webgpu::encoder
///use webgpu::tile_util

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
// struct Particle of the solver (webgpu/compute/shaders/mpm_common.wgsl) as eight vec4s:
// [0] position (metres xy, altitude z), mass (0 = inactive); [1] velocity, plastic state;
// [2..4] rows of the APIC matrix C, the particle's velocity gradient [1/s], in .xyz.
@group(2) @binding(0) var<storage, read> particles: array<vec4f>;
@group(2) @binding(1) var<uniform> avalanche: AvalancheConfig;

const PARTICLE_STRIDE_VEC4: u32 = 8u;
const SNOW_ALBEDO = vec3f(0.93, 0.95, 0.98);
const MAX_STRAIN = 0.7; // largest stretch or squeeze of an ellipsoid axis (1 -/+ this)

struct AvalancheConfig {
    xy_from_metres: vec4f, // 2x2 linear part, column-major: world.xy = (x, y) metres mapped
    offset_cws: vec2f, // world offset of metre (0, 0) minus the camera position
    radius: f32, // [m]
    colour_mode: u32, // 0 snow, 1 speed
    min_pixel_radius: f32,
    speed_max: f32, // [m/s]
    particle_count: u32,
    strain_window: f32, // [s], 0 = spheres
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) pos_cws: vec3f, // the quad point, camera-relative world
    @location(1) @interpolate(flat) centre_cws: vec3f,
    @location(2) @interpolate(flat) radius: f32, // world units, of the undeformed sphere
    @location(3) @interpolate(flat) colour: vec3f,
    // Inverse of the symmetric shape matrix S (identity for a sphere): the ellipsoid is
    // { centre + S u : |u| <= radius }.
    @location(4) @interpolate(flat) inv_shape0: vec3f,
    @location(5) @interpolate(flat) inv_shape1: vec3f,
    @location(6) @interpolate(flat) inv_shape2: vec3f,
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

// Shape of the ellipsoid: the identity plus the strain rate (symmetric part of the velocity
// gradient C) times the window, i.e. how a small ball would deform if the current strain rate
// lasted that long. The window is shortened where the strain rate is so high that an axis
// would stretch or squeeze by more than MAX_STRAIN (each eigenvalue of D is bounded by its
// Frobenius norm), and the volume is normalised to 1, so only the shape changes, not the size.
// C is in solver metres; on the map, world units are the same metres uniformly scaled, so
// the shape carries over unchanged.
fn strain_rate_shape(c0: vec3f, c1: vec3f, c2: vec3f) -> mat3x3f {
    let identity = mat3x3f(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0));
    if avalanche.strain_window <= 0.0 {
        return identity;
    }
    let c = transpose(mat3x3f(c0, c1, c2)); // rows -> the matrix C
    let d = 0.5 * (c + transpose(c));
    let norm = sqrt(dot(d[0], d[0]) + dot(d[1], d[1]) + dot(d[2], d[2]));
    let window = min(avalanche.strain_window, MAX_STRAIN / max(norm, 1e-6));
    let s = identity + window * d;
    let det = determinant(s);
    return s * (1.0 / pow(max(det, 1e-3), 1.0 / 3.0));
}

fn inverse_symmetric(m: mat3x3f) -> mat3x3f {
    let a = m[0];
    let b = m[1];
    let c = m[2];
    let r0 = cross(b, c);
    let r1 = cross(c, a);
    let r2 = cross(a, b);
    let inv_det = 1.0 / dot(a, r0);
    return transpose(mat3x3f(r0, r1, r2)) * inv_det;
}

fn hidden() -> VertexOut {
    var out: VertexOut;
    out.position = vec4f(2.0, 2.0, 2.0, 1.0); // outside the clip volume
    return out;
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance: u32) -> VertexOut {
    let base = instance * PARTICLE_STRIDE_VEC4;
    let p0 = particles[base];
    if p0.w <= 0.0 || instance >= avalanche.particle_count {
        return hidden(); // inactive
    }
    let p1 = particles[base + 1u];

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

    let shape = strain_rate_shape(particles[base + 2u].xyz, particles[base + 3u].xyz, particles[base + 4u].xyz);
    // Bounding sphere of the ellipsoid: its longest axis is at most the Frobenius norm of S.
    var extent = radius;
    if avalanche.strain_window > 0.0 {
        extent = radius * sqrt(dot(shape[0], shape[0]) + dot(shape[1], shape[1]) + dot(shape[2], shape[2]));
    }
    if dist <= extent * 1.01 {
        return hidden(); // camera inside the particle
    }

    // A quad through the centre, perpendicular to the view ray, just large enough for the
    // bounding sphere's silhouette (which grows past its radius as it comes close).
    let dir = centre / dist;
    let camera_up = camera.inv_view_matrix[1].xyz;
    var right = cross(dir, camera_up);
    right = select(cross(dir, vec3f(1.0, 0.0, 0.0)), right, dot(right, right) > 1e-8);
    right = normalize(right);
    let up = cross(right, dir);
    let half_size = extent * dist / sqrt(dist * dist - extent * extent);
    let corner = vec2f(f32(vertex_index & 1u) * 2.0 - 1.0, f32(vertex_index >> 1u) * 2.0 - 1.0);
    let pos = centre + (right * corner.x + up * corner.y) * half_size;

    var out: VertexOut;
    out.position = camera.view_proj_matrix * vec4f(pos, 1.0);
    out.pos_cws = pos;
    out.centre_cws = centre;
    out.radius = radius;
    let inv_shape = inverse_symmetric(shape);
    out.inv_shape0 = inv_shape[0];
    out.inv_shape1 = inv_shape[1];
    out.inv_shape2 = inv_shape[2];
    if avalanche.colour_mode == 1u {
        out.colour = viridis(length(p1.xyz) / avalanche.speed_max);
    } else {
        out.colour = SNOW_ALBEDO;
    }
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> FragOut {
    // The ray from the camera (the origin) through this fragment, mapped into the ellipsoid's
    // unit-sphere space: u = S^-1 (x - centre) / radius. There the hit is a unit-sphere test.
    // The closest approach q is formed directly, not as a difference of squared lengths, which
    // would cancel catastrophically at a few km for a metre-sized particle.
    let inv_shape = mat3x3f(in.inv_shape0, in.inv_shape1, in.inv_shape2);
    let dir = normalize(in.pos_cws);
    let origin_u = inv_shape * (-in.centre_cws) / in.radius;
    let dir_u = inv_shape * dir / in.radius;
    let dir_u_sq = dot(dir_u, dir_u);
    let t_closest = -dot(origin_u, dir_u) / dir_u_sq;
    let q = origin_u + dir_u * t_closest;
    let h2 = 1.0 - dot(q, q);
    if h2 < 0.0 || t_closest <= 0.0 {
        discard;
    }
    let t = t_closest - sqrt(h2 / dir_u_sq);
    let hit = dir * t;
    // Normal of the ellipsoid: the unit-sphere normal mapped by S^-T = S^-1 (S is symmetric).
    let normal = normalize(inv_shape * (origin_u + dir_u * t));
    let clip = camera.view_proj_matrix * vec4f(hit, 1.0);

    var out: FragOut;
    out.depth = clip.z / clip.w;
    out.albedo = pack4x8unorm(vec4f(in.colour, 1.0));
    out.position = vec4f(hit, length(hit));
    out.normal_enc = octNormalEncode2u16(normal);
    out.overlay = 0u;
    return out;
}
