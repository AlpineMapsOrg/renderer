# Tuning, failure modes, extending

## The parameters that actually matter

| Setting | Default | Effect |
|---|---|---|
| `domain_size_xy` | 1024 m (preset: 4000) | Simulated box. **Clamped to the tiled region** — bigger needs a bigger `GeoRegionNode.extent` (preset: 8000 m). |
| `grid_resolution_xy` | 64 (preset: 320) | With domain size, sets `dx`. Memory and grid work are `res² × layers`. |
| `grid_layers` | 16 | Node layers stored above the terrain per column (the grid follows the surface). Headroom for piles and terrain steps, not the relief. 12–16 is plenty; `dx × layers` is the maximum pile height. |
| `dt` | 0.01 s | CFL bound. Too large = explosion. |
| `substeps_per_run` | 32 (preset: 24) | Simulated time per node execution = `dt × substeps`; the overlay and the readback update once per run. |
| `substeps_per_submit` | 2 | Chunk size the run is submitted in. WebGPU has one queue, so rendering and simulation take turns on the GPU; small chunks keep the view smooth, one big chunk maximises simulated time per second. See "Cost". |
| `num_particles` | 65536 (preset: 131072) | Flow resolution. Cost is linear. |
| `release_radius` | 120 m | Start-zone size, independent of the domain. |
| `slab_thickness` | 1.5 m | Depth of released snow. |
| `youngs_modulus` | 1.4e5 Pa | Stiffness, **shared by all models**. Drives the CFL bound via wave speed. Li 2021 uses 3 MPa. |
| `dp_friction_angle` | 30° | Drucker–Prager only. Angle of repose of the granular flow; ≈ 13° for Li's cold-dense M = 0.5. |
| `ccc_m` / `ccc_beta` / `ccc_xi` / `ccc_p0_initial` | 0.7 / 0.2 / 0.002 / 3 kPa | Cam-Clay only; Li 2021 Case V. Higher M·β and βp₀ = more solid-like; ξ = brittleness. **p₀ is the one to reach for**: 3 kPa pancakes on impact, 42 kPa piles. |
| `terrain_friction` | 0.47 | Basal μ (Li 2021, real terrain). Higher = shorter runout. Pair Voellmy with ~0.155. |
| `voellmy_xi` | 4000 m/s² | Voellmy only. Turbulent drag `g|v|²/(ξ·h)`; lower ξ = more drag, lower terminal speed. |
| `splat_radius` | 6 m | **Display only.** Too small = invisible. |

## Start from a preset

The sidebar's **Material preset** combo writes a coherent parameter set — model, E, ν, ρ,
μ and the model's own parameters — and pulls `dt` under the resulting CFL bound. Seven
entries: Stomakhin 2013; Li 2021 Cases I–IV (cold dense / warm shear / sliding slab / warm
plug) and V (Vallée de la Sionne 2003, the real-avalanche back-calculation); and a
Drucker–Prager cold-dense fallback. Hand-editing anything afterwards drops it to "(custom)".

The sidebar also shows the **energy line**: `μ_eff`, the set μ, and their difference. On a
smooth plane with pure Coulomb sliding `μ_eff == μ`; on real terrain expect `μ_eff` above μ
by the internal (plastic) dissipation — ~0.1 for Stomakhin on the Breite Ries. If `μ_eff` is
*below* the set μ, energy is being created and something is wrong.

Two traps from the Li papers, already baked in so they aren't rediscovered: **warm plug has
the lowest M with the highest β**, and **ξ, not Mβ, is what separates sliding slab from
warm plug**. Also: in 3D on real terrain, Li's Cases II and III did *not* reproduce their 2D
regimes — expect re-tuning and say so in the report.

**Switching the material model reseeds.** The per-particle plastic state means different
things per model (Jp / plastic strain / α); particles seeded under one model are garbage
under another. Both panels force a reset on a model change.

## The two relationships to keep in your head

**1. Grid spacing.** `dx = domain_size / grid_resolution_xy`. Everything else follows:
the band above the terrain is `dx × grid_layers` (needs to cover piles, *not* the relief —
the grid follows the surface), and the CFL bound scales with `dx`.

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
| Snow stalls against an invisible wall | Reached the domain edge | Raise `domain_size_xy` (and the region extent if it is clamped); the grid is cheap in xy now |
| Snow flattens at a fixed height above ground | Pile reached the band ceiling `dx × (layers − 2.5)` | Raise `grid_layers` |
| Buttons greyed out | Graph has not run end to end | `Shift+R` first |
| Domain smaller than requested | Clamped to the region | Lower Select Tiles zoom for more terrain |
| Nothing seeds with release areas on | Disc doesn't overlap a 30–45° slope | Move the release lat/lon, or tick Seed anywhere — the panel now says "0 particles seeded" |
| Explodes right after picking a preset | Stiffer preset, dt above the new CFL bound | Presets pull dt under the bound automatically; if hand-set, watch the orange warning |
| Flow speeds up forever, `mu_eff` drops below the basal μ, never stops | Fixed-point bias at low-weight grid nodes (fixed 2026-09-13: rounding + 64-bit mass). If it reappears: particles-per-node × speed > 2·10⁵ saturates the momentum accumulator | Fewer particles or a larger release area (keep < ~1000 particles per cell); check `mu_eff` in the energy-line readout — healthy flows sit at or above the basal μ |
| A few particles keep moving at 20+ m/s after the mass has stopped | Lone particles on a coarse grid see almost no basal friction (their nodes barely touch the terrain layer) | Cosmetic at < 0.1 % of particles; ignore or hide with a density threshold in the overlay |

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
- **Multi-resolution grid.** The real fix, and a genuine research direction. The
  terrain-following band (2026-09-13) removed the O(N³) memory problem and lets the domain
  span the whole path, but `dx` is still set by the footprint: 4 km at 320² is 12.5 m.
  Finer cells over a whole valley need a second level, which is out of scope.

Worth stating plainly in the report: at avalanche scale, the interesting limitation is
**resolution**, not the constitutive model.

## Cost

Measured on an Apple M5 (08-domain-size-options.md §5), per run of 24 substeps:

```
ms ≈ 9 + 25 · (grid nodes / 10⁶) + 0.38 · (particles / 10³)
```

The preset (320² × 16 = 1.6 M nodes, 131 k particles) runs in ~100 ms of GPU time for
0.24 s of simulated time — 2.4× real time. Particles are the steeper axis: ~550 k is the
real-time ceiling at this substep count, whatever the grid.

**Frame rate vs simulation speed.** There is no second GPU queue in WebGPU, so a run
submitted as one 100 ms command buffer parks the next rendered frame behind it: the view
drops to ~25 fps while playing. The run is therefore submitted in chunks of
`substeps_per_submit` substeps with two chunks in flight (`MpmSolverNode::submit_chunk`),
so every frame's command buffer only ever waits for one chunk. Measured on the M5, preset
scenario, vsync at 60 Hz:

| `substeps_per_submit` | fps while playing | simulated s per wall s |
|---|---|---|
| 24 (one submit) | 25 | 2.0 |
| 8 | 53 | 1.7 |
| 4 | 53 | 1.8 |
| **2 (default)** | **60** | **1.65** |

A CPU thread would not help: the CPU side of a run is microseconds of command encoding;
the contention is on the GPU queue. A second WebGPU *device* would give a second queue,
but buffers and textures cannot be shared across devices, so the overlay texture and the
terrain would have to round-trip through the CPU every run — not worth it for 20 %.

## Where to make changes

| Want to | Touch |
|---|---|
| Different snow behaviour | a new `mpm_material_<name>.wgsl` + one case in `mpm_material.wgsl` — see below |
| Different transfer kernel (**CK-MPM** [6]) | `compute_kernel()` + the P2G/G2P loops only — the stretch goal is genuinely localised |
| Different basal friction | a new case in `mpm_friction.wgsl` (Coulomb lives there) |
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

### Adding a constitutive model

The material law is behind a runtime dispatcher (`mpm_material.wgsl`), selected by
`settings.constitutive_model`. A model is three functions and nothing else:

```
<name>_initial_state() -> f32                      plastic state of a fresh particle
<name>_stress(F, state) -> mat3x3f                 P·Fᵀ for the MLS-MPM force term
<name>_plasticity(F_trial, state) -> PlasticReturn  return mapping after the elastic predictor
```

Per-particle plastic state is **one `f32`** whose meaning the model defines (Stomakhin: Jp;
Drucker–Prager: accumulated plastic strain; Cam-Clay: plastic volumetric strain α). If a
model genuinely needs more, that is a `Particle` layout change — think twice.

E and ν (`mu_0`, `lambda_0`) are **shared** by every model. Don't add per-model stiffness
fields; put per-model recommended values in presets.

1. `webgpu/compute/shaders/mpm_material_<name>.wgsl` — the three functions, `///use mpm_common`
   at the top. Model parameters are read from `settings.*`; add them per the recipe above.
2. `mpm_material.wgsl` — `///use` the new file, add a `MATERIAL_<NAME>` constant, add a
   `case` to each of the three switches.
3. `MpmSolverNode.h` — a value in `enum ConstitutiveModel`, same number as the WGSL constant.
4. `MpmSolverNodeRenderer.cpp` and `AvalanchePanel.cpp` — append to the `Combo` string (**in
   enum order** — the combo index *is* the enum value) and show the model's parameters under
   an `if (settings.constitutive_model == ...)`.
5. `webgpu/compute/CMakeLists.txt` — add the shader to the resource list, or the include
   fails at runtime with a `qFatal`.

Basal friction is the same shape in `mpm_friction.wgsl` / `enum BasalFrictionModel`. Two
kinds of term, applied at different places: a **contact impulse** depending on `vn` (safe at
both grid and particle level — by the particle-level call `vn ≈ 0`), and a **velocity-dependent
drag** depending on `|v_t|²` (grid level only, or it double-counts). The `apply_basal_drag`
flag is `true` at exactly one call site per substep. Voellmy is the worked example.

Why runtime dispatch and not compiled variants: the preprocessor's defines are global, so
per-model shader variants would need pipeline recreation on every switch. A branch on a
uniform costs nothing — every particle takes the same path. `ComputeAvalancheTrajectoriesNode`
does exactly this for its physics/runout models.

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
