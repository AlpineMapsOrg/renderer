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

// Flow surface, pass 1 of 3 (AvalancheRenderer::prepare): splats the MLS-MPM particles onto the
// render grid with the solver's quadratic B-spline, as Stomakhin et al. 2013 rasterise their
// particles for rendering. Per node, three u32 accumulated with atomics on a zeroed buffer: the
// sum of weights, of weight * speed and of weight * height above the terrain, fixed point
// (avalanche_surface_common.wgsl). The field pass turns them into a flow depth.
// Particles more than `clearance` above the terrain (spray, jumps) are left out, faded from half
// of it: they do not stand on the ground there. Everything here is continuous in the particle
// positions, so the surface moves with the flow instead of popping from frame to frame.

///use avalanche_surface_common
///use avalanche_surface_terrain

@group(0) @binding(0) var<storage, read> particles: array<vec4f>;
@group(0) @binding(1) var<storage, read_write> accum: array<atomic<u32>>;
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<uniform> surface: SurfaceConfig;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    let index = id.x;
    if index >= surface.particle_count {
        return;
    }
    let p0 = particles[index * PARTICLE_STRIDE_VEC4]; // position, mass
    let height = p0.z - terrain_altitude(p0.xy);
    if p0.w <= 0.0 || height > surface.clearance {
        return; // inactive, or in the air
    }
    let speed = length(particles[index * PARTICLE_STRIDE_VEC4 + 1u].xyz);
    let ground = 1.0 - smoothstep(0.5 * surface.clearance, surface.clearance, height);

    // The render nodes within the B-spline's support of 1.5 kernel widths.
    let centre = (p0.xy - surface.domain_origin) / surface.spacing;
    let reach = 1.5 * surface.kernel / surface.spacing;
    let lo = max(vec2i(ceil(centre - reach)), vec2i(0));
    let hi = min(vec2i(floor(centre + reach)), vec2i(surface.nodes) - vec2i(1));
    for (var y = lo.y; y <= hi.y; y++) {
        let wy = bspline((f32(y) - centre.y) * surface.spacing / surface.kernel);
        for (var x = lo.x; x <= hi.x; x++) {
            let w = ground * wy * bspline((f32(x) - centre.x) * surface.spacing / surface.kernel);
            if w <= 0.0 {
                continue;
            }
            let base = node_index(vec2i(x, y)) * ACCUM_STRIDE_U32;
            atomicAdd(&accum[base], u32(w * WEIGHT_SCALE + 0.5));
            atomicAdd(&accum[base + 1u], u32(w * min(speed, 1000.0) * SPEED_SCALE + 0.5));
            atomicAdd(&accum[base + 2u], u32(w * max(height, 0.0) * HEIGHT_SCALE + 0.5));
        }
    }
}
