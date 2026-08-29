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

// Shared declarations for the MLS-MPM snow solver (Hu et al. 2018, Stomakhin et al. 2013).
// Every MPM kernel includes this file, so all of them declare the identical binding set
// and can therefore share a single bind group layout / bind group.

// ---------------------------------------------------------------------------------------
// Coordinate conventions
// ---------------------------------------------------------------------------------------
// * particle.position.xy  world meters, relative to the min corner of the region aabb.
// * particle.position.z   absolute altitude in meters (same units as the height texture).
// * texture uv            u grows with +x, v grows with -y (matches avalanche_trajectories_compute).
// * grid space            (position - domain origin) / dx, where the vertical origin is
//                         state.min_altitude (written by the prepare kernel) minus a margin.

struct MpmSettings {
    grid_res: vec3u, // number of grid nodes in x, y, z
    num_particles: u32,

    domain_origin: vec2f, // region-relative meters of the domain's min corner
    domain_size_xy: f32, // horizontal edge length of the domain in meters
    dx: f32, // grid spacing in meters (domain_size_xy / grid_res.x)

    region_size: vec2f, // world size of the whole region in meters
    height_texture_dim: vec2u,

    dt: f32,
    gravity: f32,
    particle_mass: f32, // normalised to 1 (see MpmSolverNode::update_gpu_settings)
    particle_volume: f32, // = 1 / density, consistent with the normalised mass

    mu_0: f32,
    lambda_0: f32,
    hardening: f32,
    critical_compression: f32,

    critical_stretch: f32,
    terrain_friction: f32,
    slab_thickness: f32,
    random_seed: u32,

    raster_dim: vec2u,
    domain_uv_min: vec2f, // region uv of the domain's min corner

    domain_uv_size: vec2f,
    _pad0: f32,
    _pad1: f32,
}

struct Particle {
    position: vec3f,
    mass: f32, // 0 marks an inactive particle
    velocity: vec3f,
    jp: f32, // plastic volume change (hardening state)
    c0: vec3f, // rows of the APIC affine velocity matrix C
    volume: f32,
    c1: vec3f,
    _p1: f32,
    c2: vec3f,
    _p2: f32,
    f0: vec3f, // rows of the elastic deformation gradient F
    _p3: f32,
    f1: vec3f,
    _p4: f32,
    f2: vec3f,
    _p5: f32,
}

// Grid quantities are accumulated with fixed point atomics because WGSL has no atomic
// float add. Particle mass is normalised to 1, so a node accumulates roughly the number
// of particles in its neighbourhood - well inside i32 range at this scale factor.
const FIXED_SCALE: f32 = 10000.0;
const FIXED_LIMIT: f32 = 2.0e9;

struct GridNode {
    mass: atomic<i32>,
    vx: atomic<i32>,
    vy: atomic<i32>,
    vz: atomic<i32>,
}

struct SimState {
    min_altitude_cm: atomic<i32>,
    max_altitude_cm: atomic<i32>,
    active_particles: atomic<u32>,
    max_speed_mm: atomic<u32>,
}

@group(0) @binding(0) var<uniform> settings: MpmSettings;
@group(0) @binding(1) var height_texture: texture_2d<f32>;
@group(0) @binding(2) var release_point_texture: texture_2d<f32>;
@group(0) @binding(3) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(4) var<storage, read_write> grid: array<GridNode>;
@group(0) @binding(5) var<storage, read_write> state: SimState;
@group(0) @binding(6) var<storage, read_write> density_raster: array<atomic<u32>>;
@group(0) @binding(7) var output_texture: texture_storage_2d<rgba8unorm, write>;

// ---------------------------------------------------------------------------------------
// Fixed point helpers
// ---------------------------------------------------------------------------------------

fn to_fixed(value: f32) -> i32 {
    // Clamping keeps a diverging simulation from wrapping the accumulator into garbage.
    return i32(clamp(value * FIXED_SCALE, -FIXED_LIMIT, FIXED_LIMIT));
}

fn from_fixed(value: i32) -> f32 { return f32(value) / FIXED_SCALE; }

// ---------------------------------------------------------------------------------------
// Terrain sampling
// ---------------------------------------------------------------------------------------

// Region-relative meters -> texture uv.
fn world_to_uv(world_xy: vec2f) -> vec2f {
    let uv_x = world_xy.x / settings.region_size.x;
    let uv_y = 1.0 - world_xy.y / settings.region_size.y;
    return vec2f(uv_x, uv_y);
}

fn load_height_texel(texel: vec2i) -> f32 {
    let dim = vec2i(settings.height_texture_dim);
    let clamped = clamp(texel, vec2i(0), dim - vec2i(1));
    return textureLoad(height_texture, vec2u(clamped), 0).r;
}

// The height texture is R32Float and therefore not filterable - interpolate manually.
fn sample_height_uv(uv: vec2f) -> f32 {
    let texel_coord = uv * vec2f(settings.height_texture_dim) - vec2f(0.5);
    let base = floor(texel_coord);
    let f = texel_coord - base;
    let b = vec2i(base);

    let h00 = load_height_texel(b);
    let h10 = load_height_texel(b + vec2i(1, 0));
    let h01 = load_height_texel(b + vec2i(0, 1));
    let h11 = load_height_texel(b + vec2i(1, 1));

    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

fn terrain_height(world_xy: vec2f) -> f32 { return sample_height_uv(world_to_uv(world_xy)); }

// Upward facing terrain normal in world space, from central differences.
fn terrain_normal(world_xy: vec2f) -> vec3f {
    let h = max(settings.dx, 1.0); // sample at grid scale, not at DEM scale
    let dhdx = (terrain_height(world_xy + vec2f(h, 0.0)) - terrain_height(world_xy - vec2f(h, 0.0))) / (2.0 * h);
    let dhdy = (terrain_height(world_xy + vec2f(0.0, h)) - terrain_height(world_xy - vec2f(0.0, h))) / (2.0 * h);
    return normalize(vec3f(-dhdx, -dhdy, 1.0));
}

// Vertical origin of the grid, derived from the terrain scan done by the prepare kernel.
fn domain_base_altitude() -> f32 {
    return f32(atomicLoad(&state.min_altitude_cm)) / 100.0 - 2.0 * settings.dx;
}

fn grid_index(node: vec3i) -> u32 {
    let res = vec3i(settings.grid_res);
    return u32((node.z * res.y + node.y) * res.x + node.x);
}

fn is_inside_grid(node: vec3i) -> bool {
    return all(node >= vec3i(0)) && all(node < vec3i(settings.grid_res));
}

// Particle world position -> continuous grid coordinates.
fn to_grid_space(position: vec3f) -> vec3f {
    let origin = vec3f(settings.domain_origin, domain_base_altitude());
    return (position - origin) / settings.dx;
}

fn to_world_space(grid_pos: vec3f) -> vec3f {
    let origin = vec3f(settings.domain_origin, domain_base_altitude());
    return grid_pos * settings.dx + origin;
}

// ---------------------------------------------------------------------------------------
// Quadratic B-spline weights (MLS-MPM)
// ---------------------------------------------------------------------------------------

struct Kernel {
    base: vec3i,
    fx: vec3f,
    w0: vec3f,
    w1: vec3f,
    w2: vec3f,
}

fn compute_kernel(grid_pos: vec3f) -> Kernel {
    var k: Kernel;
    k.base = vec3i(floor(grid_pos - vec3f(0.5)));
    k.fx = grid_pos - vec3f(k.base);
    k.w0 = 0.5 * (vec3f(1.5) - k.fx) * (vec3f(1.5) - k.fx);
    k.w1 = vec3f(0.75) - (k.fx - vec3f(1.0)) * (k.fx - vec3f(1.0));
    k.w2 = 0.5 * (k.fx - vec3f(0.5)) * (k.fx - vec3f(0.5));
    return k;
}

fn kernel_weight(k: Kernel, offset: vec3i) -> f32 {
    var wx = k.w0.x;
    if offset.x == 1 { wx = k.w1.x; } else if offset.x == 2 { wx = k.w2.x; }
    var wy = k.w0.y;
    if offset.y == 1 { wy = k.w1.y; } else if offset.y == 2 { wy = k.w2.y; }
    var wz = k.w0.z;
    if offset.z == 1 { wz = k.w1.z; } else if offset.z == 2 { wz = k.w2.z; }
    return wx * wy * wz;
}

// ---------------------------------------------------------------------------------------
// Small matrix helpers
// ---------------------------------------------------------------------------------------

fn identity3() -> mat3x3f { return mat3x3f(vec3f(1, 0, 0), vec3f(0, 1, 0), vec3f(0, 0, 1)); }

// Result A with A[i][j] = a_i * b_j.
fn outer_product(a: vec3f, b: vec3f) -> mat3x3f { return mat3x3f(a * b.x, a * b.y, a * b.z); }

fn particle_c(p: Particle) -> mat3x3f {
    // c0/c1/c2 are the rows of C; mat3x3f takes columns.
    return mat3x3f(vec3f(p.c0.x, p.c1.x, p.c2.x), vec3f(p.c0.y, p.c1.y, p.c2.y), vec3f(p.c0.z, p.c1.z, p.c2.z));
}

fn particle_f(p: Particle) -> mat3x3f {
    return mat3x3f(vec3f(p.f0.x, p.f1.x, p.f2.x), vec3f(p.f0.y, p.f1.y, p.f2.y), vec3f(p.f0.z, p.f1.z, p.f2.z));
}

fn store_c(p: ptr<function, Particle>, m: mat3x3f) {
    (*p).c0 = vec3f(m[0][0], m[1][0], m[2][0]);
    (*p).c1 = vec3f(m[0][1], m[1][1], m[2][1]);
    (*p).c2 = vec3f(m[0][2], m[1][2], m[2][2]);
}

fn store_f(p: ptr<function, Particle>, m: mat3x3f) {
    (*p).f0 = vec3f(m[0][0], m[1][0], m[2][0]);
    (*p).f1 = vec3f(m[0][1], m[1][1], m[2][1]);
    (*p).f2 = vec3f(m[0][2], m[1][2], m[2][2]);
}

// ---------------------------------------------------------------------------------------
// 3x3 signed SVD
// ---------------------------------------------------------------------------------------
// F = U * diag(sigma) * V^T with det(U) = det(V) = 1; sigma.z carries the sign of det(F).
// Obtained from a cyclic Jacobi eigendecomposition of the symmetric matrix F^T F.

struct Svd {
    u: mat3x3f,
    sigma: vec3f,
    v: mat3x3f,
}

fn svd3(f: mat3x3f) -> Svd {
    let a = transpose(f) * f;

    // Unique components of the symmetric matrix a (a_ij with i <= j).
    var a00 = a[0][0];
    var a01 = a[1][0];
    var a02 = a[2][0];
    var a11 = a[1][1];
    var a12 = a[2][1];
    var a22 = a[2][2];

    var v = identity3();

    for (var sweep = 0; sweep < 8; sweep++) {
        // Rotate the (0,1) plane, spectator index 2.
        if abs(a01) > 1e-12 {
            let theta = (a11 - a00) / (2.0 * a01);
            var t = 1.0 / (abs(theta) + sqrt(theta * theta + 1.0));
            if theta < 0.0 { t = -t; }
            let c = inverseSqrt(t * t + 1.0);
            let s = t * c;

            let n02 = c * a02 - s * a12;
            let n12 = s * a02 + c * a12;
            a00 = a00 - t * a01;
            a11 = a11 + t * a01;
            a01 = 0.0;
            a02 = n02;
            a12 = n12;
            v = mat3x3f(c * v[0] - s * v[1], s * v[0] + c * v[1], v[2]);
        }
        // Rotate the (0,2) plane, spectator index 1.
        if abs(a02) > 1e-12 {
            let theta = (a22 - a00) / (2.0 * a02);
            var t = 1.0 / (abs(theta) + sqrt(theta * theta + 1.0));
            if theta < 0.0 { t = -t; }
            let c = inverseSqrt(t * t + 1.0);
            let s = t * c;

            let n01 = c * a01 - s * a12;
            let n12 = s * a01 + c * a12;
            a00 = a00 - t * a02;
            a22 = a22 + t * a02;
            a02 = 0.0;
            a01 = n01;
            a12 = n12;
            v = mat3x3f(c * v[0] - s * v[2], v[1], s * v[0] + c * v[2]);
        }
        // Rotate the (1,2) plane, spectator index 0.
        if abs(a12) > 1e-12 {
            let theta = (a22 - a11) / (2.0 * a12);
            var t = 1.0 / (abs(theta) + sqrt(theta * theta + 1.0));
            if theta < 0.0 { t = -t; }
            let c = inverseSqrt(t * t + 1.0);
            let s = t * c;

            let n01 = c * a01 - s * a02;
            let n02 = s * a01 + c * a02;
            a11 = a11 - t * a12;
            a22 = a22 + t * a12;
            a12 = 0.0;
            a01 = n01;
            a02 = n02;
            v = mat3x3f(v[0], c * v[1] - s * v[2], s * v[1] + c * v[2]);
        }
    }

    // Sort eigenvalues (and their eigenvectors) in descending order.
    var vals = array<f32, 3>(a00, a11, a22);
    var vecs = array<vec3f, 3>(v[0], v[1], v[2]);
    for (var i = 0; i < 2; i++) {
        for (var j = 0; j < 2 - i; j++) {
            if vals[j] < vals[j + 1] {
                let tv = vals[j];
                vals[j] = vals[j + 1];
                vals[j + 1] = tv;
                let tc = vecs[j];
                vecs[j] = vecs[j + 1];
                vecs[j + 1] = tc;
            }
        }
    }

    var v0 = vecs[0];
    var v1 = vecs[1];
    var v2 = vecs[2];
    // Keep V a proper rotation.
    if determinant(mat3x3f(v0, v1, v2)) < 0.0 { v2 = -v2; }

    let sigma0 = sqrt(max(vals[0], 0.0));
    let sigma1 = sqrt(max(vals[1], 0.0));

    let fv0 = f * v0;
    let fv1 = f * v1;
    let fv2 = f * v2;

    var u0 = vec3f(1.0, 0.0, 0.0);
    if sigma0 > 1e-9 { u0 = fv0 / sigma0; }
    let u0_len = length(u0);
    if u0_len > 1e-9 { u0 = u0 / u0_len; } else { u0 = vec3f(1.0, 0.0, 0.0); }

    var u1 = fv1;
    if sigma1 > 1e-9 { u1 = fv1 / sigma1; }
    // Re-orthogonalise against u0; fall back to any orthogonal direction if degenerate.
    u1 = u1 - u0 * dot(u0, u1);
    let u1_len = length(u1);
    if u1_len > 1e-9 {
        u1 = u1 / u1_len;
    } else {
        var helper = vec3f(1.0, 0.0, 0.0);
        if abs(u0.x) > 0.9 { helper = vec3f(0.0, 1.0, 0.0); }
        u1 = normalize(cross(u0, helper));
    }

    let u2 = cross(u0, u1);
    // Signed third singular value - reproduces det(F) < 0 (inversion) correctly.
    let sigma2 = dot(u2, fv2);

    var result: Svd;
    result.u = mat3x3f(u0, u1, u2);
    result.v = mat3x3f(v0, v1, v2);
    result.sigma = vec3f(sigma0, sigma1, sigma2);
    return result;
}

// ---------------------------------------------------------------------------------------
// Constitutive model (Stomakhin et al. 2013 snow)
// ---------------------------------------------------------------------------------------

// First Piola-Kirchhoff stress premultiplied by F^T, as required by the MLS-MPM force term.
fn snow_stress(f_elastic: mat3x3f, jp: f32) -> mat3x3f {
    let svd = svd3(f_elastic);

    // Hardening: compacted snow (jp < 1) becomes stiffer.
    let h = clamp(exp(settings.hardening * (1.0 - jp)), 0.05, 20.0);
    let mu = settings.mu_0 * h;
    let lambda = settings.lambda_0 * h;

    let r = svd.u * transpose(svd.v); // rotational part of F
    let j = svd.sigma.x * svd.sigma.y * svd.sigma.z;

    let deviatoric = 2.0 * mu * (f_elastic - r) * transpose(f_elastic);
    let volumetric = lambda * j * (j - 1.0);
    return deviatoric + identity3() * volumetric;
}

struct PlasticState {
    f_elastic: mat3x3f,
    jp: f32,
}

// Push the elastic deformation gradient back into the admissible range and move the
// removed part into the plastic state jp.
fn apply_plasticity(f_trial: mat3x3f, jp: f32) -> PlasticState {
    let svd = svd3(f_trial);

    let lo = 1.0 - settings.critical_compression;
    let hi = 1.0 + settings.critical_stretch;
    let clamped = clamp(svd.sigma, vec3f(lo), vec3f(hi));

    // jp accumulates the volume change that was clamped away.
    let ratio = (svd.sigma.x / clamped.x) * (svd.sigma.y / clamped.y) * (svd.sigma.z / clamped.z);

    var result: PlasticState;
    result.jp = clamp(jp * ratio, 0.05, 20.0);
    result.f_elastic = svd.u * mat3x3f(vec3f(clamped.x, 0, 0), vec3f(0, clamped.y, 0), vec3f(0, 0, clamped.z)) * transpose(svd.v);
    return result;
}

// ---------------------------------------------------------------------------------------
// Terrain collision
// ---------------------------------------------------------------------------------------

// Coulomb friction against the terrain surface; returns the corrected velocity.
fn resolve_terrain_collision(velocity: vec3f, normal: vec3f) -> vec3f {
    let vn = dot(velocity, normal);
    if vn >= 0.0 {
        return velocity; // separating, nothing to do
    }
    let vt = velocity - normal * vn;
    let vt_len = length(vt);
    if vt_len <= -settings.terrain_friction * vn {
        return vec3f(0.0); // sticking
    }
    return vt * (1.0 + settings.terrain_friction * vn / vt_len);
}
