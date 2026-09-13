# Data layout and conventions

The fiddly stuff. Get any of this wrong and the symptom is garbage output, not a compile
error — so it is written down.

## Coordinate conventions

| Quantity | Convention |
|---|---|
| `particle.position.xy` | World metres, **relative to the min corner of the region AABB** |
| `particle.position.z` | **Absolute altitude** in metres (same units as the height texture) |
| Texture UV | u grows with +x, **v grows with −y** (`v = 1 − y/region_size.y`) |
| Grid space | `(position − domain_origin) / dx`; vertical origin from `SimState.min_altitude − 2·dx` |
| Settings lat/lon | `glm::dvec2(latitude, longitude)`, converted in `update_gpu_settings()` |

The v-flip matches `avalanche_trajectories_compute.wgsl` and the height texture (row 0 =
north). `world_to_uv()` is the only place it is applied — go through it.

**Why lat/lon for domain and release centres:** they used to be normalized [0,1] within the
region. That breaks the moment the region changes, because `SelectTilesNode` snaps to tile
boundaries — the same normalized value lands on different ground. Geographic anchors are
converted where the region bounds are known, so tile snapping cannot move a scenario
relative to the terrain.

## `Particle` — 128 bytes

```wgsl
struct Particle {
    position: vec3f,  mass: f32,     // mass = 0 marks an INACTIVE particle
    velocity: vec3f,  plastic_state: f32,  // per model: Stomakhin Jp (1), DP plastic strain (0), CCC alpha (−asinh(p₀/K)/ξ)
    c0: vec3f,        volume: f32,   // c0..c2 = rows of the APIC affine matrix C
    c1: vec3f,        _p1: f32,
    c2: vec3f,        _p2: f32,
    f0: vec3f,        _p3: f32,      // f0..f2 = rows of the deformation gradient F
    f1: vec3f,        _p4: f32,
    f2: vec3f,        _p5: f32,
}
```

Every `vec3f` sits at a multiple of 16 — WGSL aligns vec3 to 16 bytes, so the padding floats
are not optional. Useful scalars are tucked into those slots rather than wasted. Total is
128 B, a nice power of two.

C and F are stored as **rows**; `mat3x3f` takes **columns**. `particle_c()` / `particle_f()`
and `store_c()` / `store_f()` do the transpose. Don't bypass them.

## `GridNode` — 16 bytes

```wgsl
struct GridNode {
    mass_lo: atomic<u32>, mass_hi: atomic<u32>,      // 64-bit mass, scale 2^20
    vx: atomic<i32>, vy: atomic<i32>, vz: atomic<i32> // momentum, scale 1e4
}
```

20 bytes, stride 5 u32 (`GRID_NODE_STRIDE_U32`). `v*` holds **momentum** after P2G and
**velocity** after the grid update — same slots, reused.

## Fixed-point atomics

WGSL has no atomic float add, so mass and momentum accumulate as scaled integers:

```wgsl
const MOMENTUM_SCALE: f32 = 10000.0;   // i32, rounded, clamped to ±2e9 before every add
const MASS_SCALE: f32 = 1048576.0;     // 2^20, u32 lo/hi pair with carry
```

Clamping matters: without it a diverging simulation wraps the accumulator and produces
garbage that looks like a physics bug rather than an overflow.

### Round, don't truncate — and give the mass its own scale (2026-09-13)

The first version truncated every contribution (`i32(x · 1e4)`) and kept the mass in one
`i32` at the same 1e-4 quantum. That is the bug behind "the simulation never cools down"
(see [06-verification.md](06-verification.md#bugs-found-and-fixed-along-the-way), bug 8).
The mechanism, because it is not obvious:

- Node velocity is `Σ q(w·v) / Σ q(w)` with `q` the quantiser. Truncation toward zero takes
  up to one unit off *each* contribution. For a mass contribution of `w = 1e-3` that is up
  to 10 %; for the matching momentum `w·v` with `v = 10 m/s` it is 1 %. The ratio is
  therefore **biased high** at every low-weight node, by roughly `(units lost)/(units) ·
  (1 − 1/|v|)` — a systematic, per-step, dt-independent gain.
- A 1.5 m slab in a 12.5 m cell is 0.12 cells thick, so the third z layer of its stencil
  has weights of 1e-3…1e-2 for *every* particle. Those nodes are not a fringe, they are a
  whole layer of the flow.
- APIC closes the loop: `C = 4/dx² Σ w vᵢ ⊗ dpos` pulls the over-fast node velocities into
  the particle's affine field, P2G hands them back to the grid, and the flow pumps energy
  without limit. PIC (no C) only shows a mild drift; that is how C was first (wrongly)
  blamed.

Rounding makes the sums unbiased; the finer mass scale removes the remaining ratio noise
where the mass is only a few units. `test_sheet_fixed_point.py` emulates both schemes:
the old one runs away by 335 % in 20 s, the new one is within 0.1 % of the rigid block.

Why the mass is 64-bit but the momentum is not: mass is non-negative, so the `atomicAdd`
carry trick from `SimState` works and the range is unlimited. Momentum is signed, and a
signed 64-bit emulation would double the atomics in the hot loop. Its range at 1e-4 is
±2·10⁵ momentum units per node, i.e. **particles-per-node × speed < 2·10⁵** — 450
particles per cell (131 k in a 120 m disc at dx 12.5) allow ~450 m/s, far above anything
physical. A very dense seeding is the one way to hit it; see [05-tuning.md](05-tuning.md).

### The mass normalisation trick

This is the part worth explaining in the report.

Real avalanche masses are large — a grid node can easily see ~10⁶ kg. At scale 1e4 that
overflows i32 immediately. Choosing a scale factor that works for all scenarios is not
possible if masses vary by orders of magnitude.

**Fix:** set particle mass = 1 and particle volume = 1/ρ.

Why that is legitimate: in the MPM equations, scaling every particle mass *and* every
particle volume by the same constant k leaves the dynamics unchanged. Grid mass and grid
momentum both scale by k, so `v = mom/mass` is invariant; the stress term scales with volume,
so it tracks. Gravity is an acceleration and unaffected. It is a **change of mass units**,
not an approximation.

With k = 1/m_real: mass' = 1, volume' = V_real/m_real = 1/ρ. Grid mass then reads roughly
"particles in the neighbourhood" — O(10–1000) — and the momentum accumulator's range is a
statement about particle counts and speeds, not about how much snow the scenario contains.

## `SimState` — 56 bytes

```wgsl
struct SimState {
    min_altitude_cm: atomic<i32>,    // grid vertical origin        - written once per reset
    max_altitude_cm: atomic<i32>,    //                               - written once per reset
    active_particles: atomic<u32>,   // seeded successfully          - written once per reset
    max_speed_mm:     atomic<u32>,   // fastest particle this run    - zeroed before each run
    plastic_particles: atomic<u32>,  // plastic state left initial   - zeroed before each run
    _reserved: atomic<u32>,
    sum_x_lo / sum_x_hi,             // 64-bit sums over active particles, zeroed each run:
    sum_y_lo / sum_y_hi,             //   position * 1e4 (0.1 mm) ...
    sum_z_lo / sum_z_hi,
    sum_speed_sq_lo / _hi,           //   ... and |v|^2 * 1e5
}
```

**Why 64-bit.** A fixed-point sum over 10⁵ particles overflows a `u32` at any useful
precision, and the obvious dodge — summing `value/N` so the total is the mean — quantises
each particle's contribution to `floor(v²/N × scale)`, which is **0 below 3.6 m/s** at
N = 131072. That silently dropped ~30 % of kinetic energy and read zero for slow flows.
The lo/hi pair with carry detection (see `03-shaders.md`, `mpm_splat`) costs one extra
branch per add and has no such limit. Reassembled on the CPU by `wide()`.

Written from the CPU on reset via `RawBuffer::write()` with ±INT_MAX so the atomics
converge; slots 3–4 are zeroed by a 2-element `write()` before every non-reset run.
**Read back** after each run via `read_back_async()` from the work-done callback into
`MpmSolverNode::last_state()`; the value therefore describes the previous completed run.

The buffer is **allocated once in the constructor and never reallocated** — an async
readback in flight while `ensure_resources()` recreated it would be a use-after-free. It
is fixed-size, so there is no reason to reallocate it anyway.

## Uniform — 176 bytes

Mirrored between `MpmSolverSettingsUniform` (C++) and `MpmSettings` (WGSL). A
`static_assert(sizeof(...) == 176)` catches size drift, **but not field reordering** — if you
add a field, change both sides and check the offsets by hand.

Rules that make the two agree:

- `vec3` aligns to 16, `vec2` to 8, scalars to 4
- struct alignment = its largest member's (16 here, from the leading `vec3u`)
- struct size must be a multiple of its alignment → 176 ✓
- glm types in C++ are tightly packed at 4-byte alignment, which happens to match because
  every `vec2` in this layout already sits at an 8-byte offset

Field groups, in order: grid/particle counts · domain origin+size+dx · region size + height
texture dims · dt, gravity, mass, volume · μ₀, λ₀, ξ, θ_c · θ_s, friction, slab, seed ·
raster dims + domain UV · domain UV size, seed_anywhere, splat radius · density reference,
release centre x/y, release radius · constitutive_model, basal_friction_model, voellmy_xi, dp_alpha ·
ccc_m, ccc_beta, ccc_xi, ccc_p0_initial.

No pad slots left. The next field grows the struct to 192 B.

## Buffer sizes

| Buffer | Size |
|---|---|
| particles | `num_particles × 32` u32 (128 B each) |
| grid | `res.x·res.y·res.z × 4` u32 (16 B each) |
| state | 4 u32 |
| density raster | `raster_resolution²` u32 |

Grid memory is the one that bites: **O(N³)**. 64³ = 4 MB, 128³ = 33 MB, 256³ = 268 MB.
128³ is the practical sweet spot.

All buffers use `Storage | CopyDst | CopySrc` — `CopyDst` is required for
`wgpuCommandEncoderClearBuffer`, `CopySrc` lets `ExportNode` read them.

## Reallocation

`ensure_resources()` only reallocates when particle count, grid resolution or raster
resolution actually change, and **forces a reset when it does** — fresh buffers hold garbage.
Changing those settings therefore restarts the simulation; changing material parameters does
not.
