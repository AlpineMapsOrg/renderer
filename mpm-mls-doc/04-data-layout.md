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
    velocity: vec3f,  jp:   f32,     // jp = plastic volume change (hardening state)
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
struct GridNode { mass: atomic<i32>, vx: atomic<i32>, vy: atomic<i32>, vz: atomic<i32> }
```

Holds **momentum** after P2G and **velocity** after the grid update — same slots, reused.

## Fixed-point atomics

WGSL has no atomic float add, so mass and momentum accumulate as scaled integers:

```wgsl
const FIXED_SCALE: f32 = 10000.0;
const FIXED_LIMIT: f32 = 2.0e9;      // clamp before every add
```

Clamping matters: without it a diverging simulation wraps the accumulator and produces
garbage that looks like a physics bug rather than an overflow.

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
"particles in the neighbourhood" — O(10–1000) — which is safely inside i32 at scale 1e4
regardless of how much snow the scenario actually contains.

## `SimState` — 16 bytes

```wgsl
struct SimState {
    min_altitude_cm: atomic<i32>,    // grid vertical origin
    max_altitude_cm: atomic<i32>,
    active_particles: atomic<u32>,   // written but not read back yet
    max_speed_mm:     atomic<u32>,
}
```

Written from the CPU on reset via `RawBuffer::write()` with ±INT_MAX so the atomics
converge. `active_particles` and `max_speed_mm` are populated but **not currently read back**
— readback is async and would complicate `run_impl()`. Low-hanging fruit if diagnostics are
wanted.

## Uniform — 144 bytes

Mirrored between `MpmSolverSettingsUniform` (C++) and `MpmSettings` (WGSL). A
`static_assert(sizeof(...) == 144)` catches size drift, **but not field reordering** — if you
add a field, change both sides and check the offsets by hand.

Rules that make the two agree:

- `vec3` aligns to 16, `vec2` to 8, scalars to 4
- struct alignment = its largest member's (16 here, from the leading `vec3u`)
- struct size must be a multiple of its alignment → 144 ✓
- glm types in C++ are tightly packed at 4-byte alignment, which happens to match because
  every `vec2` in this layout already sits at an 8-byte offset

Field groups, in order: grid/particle counts · domain origin+size+dx · region size + height
texture dims · dt, gravity, mass, volume · μ₀, λ₀, ξ, θ_c · θ_s, friction, slab, seed ·
raster dims + domain UV · domain UV size, seed_anywhere, splat radius · density reference,
release centre x/y, release radius.

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
