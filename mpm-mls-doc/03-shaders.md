# The eight kernels

All in `webgpu/compute/shaders/`. Every one starts with `///use mpm_common` (weBIGeo's
shader preprocessor include directive), so they all declare the **identical binding set** —
which is why one bind group layout and one bind group serve all of them.

## Dispatch order

One node execution, all inside a **single compute pass**:

```
[buffer clear] density_raster                 (outside the pass)
begin compute pass
  if reset:  mpm_prepare  →  mpm_seed
  repeat substeps_per_run times:
      mpm_clear_grid → mpm_p2g → mpm_grid_update → mpm_g2p
  mpm_splat → mpm_rasterize
end pass
submit → wgpuQueueOnSubmittedWorkDone → complete_run()
```

**Why one pass matters:** dispatches within a single WebGPU compute pass are ordered and see
each other's storage writes. That is exactly the MPM dependency chain, so no pass churn is
needed between stages. The grid is cleared by a *kernel* rather than
`wgpuCommandEncoderClearBuffer` precisely because a buffer clear cannot happen inside a pass
and would force it to end and restart every substep.

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
```

Unused bindings in a given kernel are fine; the reverse (using something not in the layout)
is not.

---

## `mpm_prepare` — 16×16×1, over grid XY

Runs once per reset, before seeding. Samples terrain at each grid column's centre and
`atomicMin`/`atomicMax` the altitude (in cm, as i32) into `SimState`.

Establishes the grid's vertical origin without a CPU readback. `SimState` is pre-seeded from
the CPU with ±INT_MAX so the atomics converge — a zero-cleared buffer would make `atomicMin`
stick at 0.

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
Initialised with `F = I`, `C = 0`, `Jp = 1`.

Uses `///use random` (PCG hash from the existing `random.wgsl`).

## `mpm_clear_grid` — 256×1×1, over grid nodes

Zeroes mass and the three momentum components. Trivial, but see the note above on why it is
a kernel.

## `mpm_p2g` — 256×1×1, over particles

Stage 1. Skips `mass <= 0`.

```
gp     = to_grid_space(position)
kernel = compute_kernel(gp)
stress = snow_stress(F, jp)

stress_term = −dt · volume · (4/dx²) · stress
affine      = stress_term + mass · C

for offset in 3×3×3:
    dpos = (offset − fx) · dx
    w    = kernel_weight(...)
    atomicAdd(mass, to_fixed(w · mass))
    atomicAdd(v*,   to_fixed(w · (mass·velocity + affine·dpos)))
```

The single `affine · dpos` product is the MLS-MPM payoff — no separate weight-gradient force
term (Hu et al. [3]).

27 nodes × 4 atomics = **108 atomic adds per particle per substep**. This is the hot loop.

## `mpm_grid_update` — 4×4×4, over grid nodes

Stage 2.

```
mass = from_fixed(...)
if mass <= 1e-9:  zero the velocity slots and return   # so G2P never reads stale data
v = momentum / mass
v.z −= gravity · dt

world = to_world_space(node)
if world.z < terrain_height(world.xy):
    v = resolve_terrain_collision(v, terrain_normal(world.xy))

clamp at domain walls (2-node margin, only blocks outflow)
store v back into the momentum slots
```

Velocity is written into the momentum slots — G2P then reads nodes as plain velocities. The
explicit zeroing of empty nodes is what makes that safe.

## `mpm_g2p` — 256×1×1, over particles

Stages 3+4 fused.

```
for offset in 3×3×3:
    dpos = offset − fx                       # GRID units here, not world
    v_new += w · node_velocity
    C_new += 4·(1/dx) · w · outer(node_velocity, dpos)

F_trial = (I + dt·C_new) · F
F, jp   = apply_plasticity(F_trial, jp)
position += dt · v_new

if position.z < terrain_height:              # particle-level collision
    position.z = terrain_height
    velocity = resolve_terrain_collision(velocity, normal)

clamp into the domain box, zeroing the velocity component that hit
atomicMax(state.max_speed_mm, ...)
```

Watch the `dpos` asymmetry against P2G — world units there, grid units here with a `4/dx`
factor. Both match the reference formulation; mixing them up gives plausible-looking but
wrong results.

## `mpm_splat` — 256×1×1, over particles

Projects particles top-down into the density raster. Each particle is drawn as a **disc** of
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
