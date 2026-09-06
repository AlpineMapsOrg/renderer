# Tuning, failure modes, extending

## The parameters that actually matter

| Setting | Default | Effect |
|---|---|---|
| `domain_size_xy` | 1024 m | Simulated box. **Clamped to the tiled region** — bigger needs a bigger region. |
| `grid_resolution_xy/z` | 64 (preset: 128) | With domain size, sets `dx`. Memory is O(N³). |
| `dt` | 0.01 s | CFL bound. Too large = explosion. |
| `substeps_per_run` | 32 (preset: 24) | Simulated time per node execution = `dt × substeps`. |
| `num_particles` | 65536 (preset: 131072) | Flow resolution. Cost is linear. |
| `release_radius` | 120 m | Start-zone size, independent of the domain. |
| `slab_thickness` | 1.5 m | Depth of released snow. |
| `youngs_modulus` | 1.4e5 Pa | Stiffness. Drives the CFL bound via wave speed. |
| `terrain_friction` | 0.4 | Coulomb μ. Higher = shorter runout. |
| `splat_radius` | 6 m | **Display only.** Too small = invisible. |

## The two relationships to keep in your head

**1. Grid spacing.** `dx = domain_size / grid_resolution_xy`. Everything else follows:
vertical extent is `dx × grid_resolution_z` (must cover the terrain relief), and the CFL
bound scales with `dx`.

**2. CFL.** MPM is explicit, so the timestep is bounded by how far a wave travels in one
step:

```
c    = √(E/ρ)                 ≈ 18.7 m/s at defaults
dt  ≲ 0.1 · dx / c
```

The panel computes and displays this, and warns in orange when `dt` exceeds it. At dx = 12.5 m
that gives dt ≲ 0.067 s, so the 0.01 s default has comfortable margin.

Raising E raises the wave speed and *lowers* the allowed dt — stiffer snow is more expensive,
not just different.

## Failure modes

| Symptom | Cause | Fix |
|---|---|---|
| Nothing visible at all | Splat radius too small for the raster scale, or nothing seeded | Tick **Seed anywhere**; raise splat radius; check "Output texel: X m" |
| Snow vanishes / flickers | dt above CFL | Lower dt or raise grid resolution (watch the orange warning) |
| Snow behaves like water | Slab is sub-cell at this `dx` | Shrink the domain, or accept it — see below |
| Snow sinks into terrain | Vertical range too small, or terrain relief exceeds `dx × res_z` | Raise `grid_resolution_z` |
| Buttons greyed out | Graph has not run end to end | `Shift+R` first |
| Domain smaller than requested | Clamped to the region | Lower Select Tiles zoom for more terrain |
| Nothing seeds with release areas on | Disc doesn't overlap a 30–45° slope | Move the release lat/lon, or tick Seed anywhere |

## The resolution problem (be honest about this in the report)

Stomakhin's parameters were tuned for **metre-scale** snow in film VFX. Here a 1.5 m slab
sits in 12–16 m cells — the released layer is **an order of magnitude thinner than one grid
cell**. P2G smears it vertically across the stencil, so the material never really behaves
like a cohesive slab, and the result reads as a viscous granular flow rather than a slab
avalanche with a fracture line.

This is inherent to the scale gap, not a bug. Options, none free:

- **Shrink the domain** (256–512 m → 2–4 m cells). Physically better, but then it cannot run
  out into the valley — the thing the simulation is *for*.
- **Thicken the slab** to a few cells. Visually better, physically dishonest about how much
  snow is involved.
- **Sparse / multi-resolution grid.** The real fix, and a genuine research direction —
  a dense O(N³) grid over a whole valley is the actual constraint.

Worth stating plainly in the report: at avalanche scale, the interesting limitation is
**resolution**, not the constitutive model.

## Where to make changes

| Want to | Touch |
|---|---|
| Different snow behaviour | `snow_stress()` / `apply_plasticity()` in `mpm_common.wgsl` |
| Different transfer kernel (**CK-MPM** [6]) | `compute_kernel()` + the P2G/G2P loops only — the stretch goal is genuinely localised |
| Better collision | `resolve_terrain_collision()`, plus the particle branch in `mpm_g2p` |
| Different visualisation | `mpm_splat` / `mpm_rasterize`, or bypass both — see below |
| Add a location | One `Scenario` entry in `AvalanchePanel`'s constructor |
| New tunable | Settings struct → uniform (**both sides**) → `update_gpu_settings()` → serialize/deserialize → panel |

### Adding a uniform field

Order matters and the compiler will not catch mistakes:

1. `MpmSolverSettings` (C++ header)
2. `MpmSolverSettingsUniform` **and** `MpmSettings` in `mpm_common.wgsl` — same offsets
3. Update the `static_assert` size
4. Set it in `update_gpu_settings()`
5. `serialize_settings()` / `deserialize_settings()`
6. Panel control

Reuse an existing pad float if the size allows — that avoids touching the layout at all.

## The 3D renderer (next big thing)

The current output is a top-down density projection. The proposal's Phase 5 wants real
particle rendering. What is already in place:

- The node exposes `particle buffer` as an output socket — a renderer can bind it directly,
  **no CPU readback**.
- `Particle` is 128 B with position at offset 0, so a vertex shader can read positions with
  a stride and nothing else.

What is needed: a new **Renderer** in `webgpu/engine/` (not an Overlay — overlays are
screen-space and cannot depth-test against terrain), registered on `webgpu_engine::Context`,
instantiated in `RenderingContext::initialize()`, invoked from `Window::paint()` after the
tile mesh pass so it depth-tests against terrain. See `docs/webgpu_engine.md` and
`CloudRenderer` as the closest existing analogue.

Rendering technique options are surveyed in the Notion sub-page; billboards are the
recommended starting point, with screen-space surface reconstruction (Preiner [8], Wu et
al. [9]) as the upgrade.
