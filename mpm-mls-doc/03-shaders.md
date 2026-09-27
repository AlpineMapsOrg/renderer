# The seven kernels

All in `webgpu/compute/shaders/`. Every one starts with `///use mpm_common` (weBIGeo's
shader preprocessor include directive), so they all declare the **identical binding set** —
which is why one bind group layout and one bind group serve all of them.

## Dispatch order

One node execution, all inside a **single compute pass**:

```
[buffer clear] density_raster; on reset also grid + tile_flags     (outside the pass)
begin compute pass                                                  (one per chunk)
  if reset:  mpm_prepare  →  mpm_seed
  repeat substeps_per_submit times:
      mpm_p2g → mpm_grid_update → mpm_g2p
  last chunk: mpm_splat → mpm_rasterize
end pass
last chunk: copy state → readback buffer
submit → wgpuQueueOnSubmittedWorkDone → next chunk, or map readback + complete_run()
```

**Why one pass matters:** dispatches within a single WebGPU compute pass are ordered and see
each other's storage writes. That is exactly the MPM dependency chain, so no pass churn is
needed between stages.

**No clear pass (2026-09-27).** There used to be a `mpm_clear_grid` kernel zeroing every node
every substep. The grid update is the last reader of the P2G accumulators, so it now zeroes
the nodes it consumed itself; together with the active-tile flags (below) the grid work
follows the snow instead of the domain. See [09-performance-analysis.md](09-performance-analysis.md) §9.1.

## Bindings (shared by all)

```
0  uniform    MpmSettings
1  texture    height_texture           R32Float, unfilterable
2  texture    release_point_texture    RGBA8Unorm, alpha > 0 marks a release cell
3  storage    particles      array<Particle>       read_write
4  storage    grid           array<GridNode>       read_write, atomic<i32>
5  storage    state          SimState              read_write, atomics
6  storage    density_raster array<atomic<u32>>    read_write
7  storage    output_texture rgba8unorm            write
8  storage    column_floor   array<i32>            read_write (written by prepare)
9  storage    grid_velocity  array<vec4f>          read_write (grid update → G2P)
10 storage    tile_flags     array<atomic<u32>>    read_write (P2G → grid update)
```

Unused bindings in a given kernel are fine; the reverse (using something not in the layout)
is not.

---

## Modules (not kernels)

Three files carry no entry point; kernels `///use` them.

| Module | Provides | Used by |
|---|---|---|
| `mpm_common` | bindings, structs, terrain sampling, B-spline kernel, SVD, matrix helpers | all |
| `mpm_material` | `material_initial_state()`, `material_plasticity()` (which also returns τ = P·Fᵀ of the returned state) — a `switch` on `settings.constitutive_model` over `mpm_material_stomakhin` / `mpm_material_drucker_prager` / `mpm_material_ccc` | seed, g2p, splat |
| `mpm_friction` | `resolve_terrain_collision(v, n, apply_basal_drag)` — a `switch` on `settings.basal_friction_model`; Coulomb, Voellmy | grid_update (`true`), g2p (`false`) |

`mpm_material` and `mpm_friction` are separate on purpose: internal friction (M, inside the
material law) and basal friction (μ, a boundary condition) are different things and are
selected independently. The include mechanism is pragma-once, so a module can `///use
mpm_common` for itself without double-declaring the bindings.

## `mpm_prepare` — 16×16×1, over grid XY

Runs once per reset, before seeding. Samples the terrain at each grid column's node and
does two things: `atomicMin`/`atomicMax` the altitude (in cm, as i32) into `SimState` for
the readout, and writes the column's **band floor**
`column_floor[column] = floor(altitude / dx) − 2` — the absolute z index of the first
stored layer. See "The terrain-following grid" in
[04-data-layout.md](04-data-layout.md#the-terrain-following-grid-2026-09-13).

`SimState` is pre-seeded from the CPU with ±INT_MAX so the atomics converge — a
zero-cleared buffer would make `atomicMin` stick at 0.

## `mpm_seed` — 256×1×1, over particles

One particle per thread. Rejection-samples inside the **release disc**:

```
angle    = rand · 2π
distance = √rand · release_radius        # √ keeps samples uniform over area
candidate = release_centre + (cos, sin)·distance
```

Rejects candidates outside the domain (with a 2·dx margin — a particle outside the box gets
clamped onto its wall immediately). Then requires a release-point-texture hit unless
`seed_anywhere` is set. Up to 32 attempts; failures leave the particle **inactive**
(`mass = 0`), which every later stage skips.

Particle z = terrain height + `rand · slab_thickness`, so the slab has real depth.
Initialised with `F = I`, `C = 0`, `plastic_state = material_initial_state()` (Jp = 1 for
Stomakhin — the initial state is model-dependent, which is why it is a dispatcher call).

Uses `///use random` (PCG hash from the existing `random.wgsl`).

## `mpm_p2g` — 256×1×1, over particles

Stage 1. Skips `mass <= 0`.

```
gp     = to_grid_space(position)
kernel = compute_kernel(gp)
mark_tiles(kernel.base.xy)                      # flag the ≤ 4 tiles of 8×8 columns the stencil reaches
tau    = particle_kirchhoff(p)                  # P·Fᵀ, stored by the previous G2P — no SVD here

stress_term = −dt · particle_volume · (4/dx²) · tau
affine      = stress_term + mass · C

for offset in 3×3×3:
    slot = grid_slot(base + offset)                     # −1 outside the box or the column's band → skip
    dpos = (offset − fx) · dx
    w    = kernel_weight(...)
    add_node_mass(cell, w · mass)                       # u32 lo/hi at 2^20, carry from atomicAdd's return
    atomicAdd(v*, to_fixed(w · (mass·velocity + affine·dpos)))   # i32 at 1e4, rounded
```

Both quantise with `round`, never truncation — see the fixed-point section of
[04-data-layout.md](04-data-layout.md#round-dont-truncate--and-give-the-mass-its-own-scale-2026-09-13)
for the blow-up that truncation caused.

The single `affine · dpos` product is the MLS-MPM payoff — no separate weight-gradient force
term (Hu et al. [3]).

27 nodes × 4 atomics (+1 on a mass carry, rare) = **108 atomic adds per particle per substep**. This is the hot loop — and with
hundreds of particles per cell, the atomics to the same node serialise ([09-performance-analysis.md](09-performance-analysis.md) §2.2).

## `mpm_grid_update` — 8×8×1, one workgroup per tile of columns

Stage 2. Each thread owns one column and walks its layers (the band, not altitudes).

```
thread 0: active = tile_flags[tile];  workgroupUniformLoad → all threads
if !active: return                                     # no particle touched this tile this substep
thread 0: tile_flags[tile] = 0                         # P2G of the next substep flags it again
for layer in 0 .. res.z:
    cell = layer · res.x · res.y + column(x, y)
    read mass (lo/hi) and momentum; zero them unless already zero   # replaces the clear pass
    if mass <= 0:  grid_velocity[cell] = 0; continue
    v = momentum / mass
v.z −= gravity · dt

world = column_node_world(node_xy, layer)             # altitude = (column_floor + layer) · dx
if world.z < terrain_height(world.xy):
    v = resolve_terrain_collision(v, terrain_normal(world.xy), true)   # basal drag applied here

    clamp at domain walls (2-node margin, only blocks outflow)
    grid_velocity[cell] = v                            # plain floats for G2P
```

Nodes outside flagged tiles are never visited: their accumulators are still zero from the
last time they were consumed, and no particle's stencil reads their velocity this substep.

## `mpm_g2p` — 256×1×1, over particles

Stages 3+4 fused.

```
for offset in 3×3×3:
    dpos = offset − fx                       # GRID units here, not world
    v_new += w · node_velocity
    C_new += 4·(1/dx) · w · outer(node_velocity, dpos)

F_trial = (I + dt·C_new) · F
F, plastic_state, tau = material_plasticity(F_trial, plastic_state)   # tau from the same SVD, stored for P2G
position += dt · v_new

if position.z < terrain_height:              # particle-level collision
    position.z = terrain_height
    velocity = resolve_terrain_collision(velocity, normal, false)  # contact impulse only

clamp into the domain box, zeroing the velocity component that hit
atomicMax(state.max_speed_mm, ...)
```

Watch the `dpos` asymmetry against P2G — world units there, grid units here with a `4/dx`
factor. Both match the reference formulation; mixing them up gives plausible-looking but
wrong results.

## `mpm_splat` — 256×1×1, over particles

Also, once per run: counts particles whose `plastic_state` has left its initial value into
`SimState.plastic_particles`, and sums position and `|v|²` over active particles for the
energy-line test. The sums are **64-bit** as lo/hi `u32` pairs — WGSL has no 64-bit atomics,
and `atomicAdd` returns the old value, so a wrap is detectable:

```
let old = atomicAdd(&lo, x);   if old > 0xFFFFFFFF − x { atomicAdd(&hi, 1u); }
```

All summands are non-negative, so unsigned suffices. Then projects
particles top-down into the density raster. Each particle is drawn as a **disc** of
`splat_radius_texels` (capped at 8), not a single texel:

```
for dy, dx in the bounding square:
    if dx² + dy² > r²:  continue
    atomicAdd(density_raster[...], 1)
```

> Why: with a 1024 m domain on a 1024-texel raster, single-texel splatting put 65 k
> particles into ~6% of a million texels at count 1 — isolated 1-metre pixels, invisible
> from map viewing distance. A particle represents a *parcel* of snow, not a point.

## `mpm_rasterize` — 16×16×1, over raster texels

Density → RGBA.

```
density = clamp(count / density_reference, 0, 1)
color   = mix(pale blue, white, √density)
alpha   = clamp(0.25 + 0.70·density, 0, 0.95)
```

`density_reference` is computed CPU-side from particle count, splat area and the **release
disc** area (with a spread tolerance), not from the domain. Basing it on the domain would
mean a small avalanche inside a large box saturates everywhere. Deriving it rather than
hard-coding a constant means the display rescales itself when particle count or resolution
change.

Empty texels are written fully transparent.
