# 15 — Entrainment

How the avalanche picks up snow along its path, and where each part lives. Research and the
earlier particle-cover idea: Notion "Entrainment with a particle snow cover — research and
feasibility (12.10.2026)"; this page is what was built.

**Scope:** everywhere the solver runs.

| Context | Snow cover | Default |
|---|---|---|
| AvaFrame sandbox | AvaFrame's entrainment area of the dataset (the ENT shapefile, 0.3 m) | off, so the comparison with com1DFA's null tests stays as it was |
| Map, AvaFrame scenarios | the same area, resampled onto the map region with the release | erosion |
| Map, generic scenarios | one thickness (0.3 m, perpendicular to the slope) on every slope up to 50° | erosion |

Everywhere the cover leaves out the release itself, as com1DFA removes the overlap of
entrainment and release areas for each run.

## How it works

Implemented like com1DFA's entrainment (Tonnel et al. 2023, and the com1DFA theory
documentation): the cover is a field per grid column, and the flow takes snow from it as a
source term. This is due to the cost of the alternative, a cover of dormant particles waiting
along the path (Kyburz et al. 2024's "activation"). The whole cover has to exist as particles
from the start, although the flow only ever touches a strip of it: a 0.3 m cover over the
generic map's 4 km domain is 16 million particle masses at Medium detail.

1. **At the reset** (`mpm_prepare`), each column gets the cover of its dx × dx cell:
   4 × 4 samples of `cover_depth_at()` (vertical depth: the texture, or the uniform
   thickness / cos(slope) up to the slope limit), without the samples under the release
   (`under_release()`). Stored in `column_stats[column].cover` [m³ of cover], summed into
   `state.cover_volume`.
2. **Every substep, in the grid update**, each column that holds flow (`entrain_column()`):
   the flow depth h (the column's mass as volume per area, as for Voellmy) and speed |u|
   (mass-weighted over the column's nodes). Below `entrainment_min_speed` (1 m/s) nothing is
   taken.
   - **Plowing:** once h ≥ `plowing_depth` (0.1 m), the front takes the column's whole cover.
   - **Erosion** (com1DFA's default): the flow erodes τ_b |u| / e_b of cover mass per area and
     second; τ_b = μ ρ g h cos θ (Coulomb), plus ρ g |u|² / ξ under Voellmy; e_b = 5000 J/kg.
     Per substep that is min(cover, τ_b |u| / e_b · dt · dx² / ρ_cover) m³.
3. **Eroded snow becomes particles** from a pool after the release particles. Each has the mass
   of a release particle: a cover volume of V · ρ_snow / ρ_cover, V the real volume per release
   particle (`seeded_particle_volume()`), ρ_cover = 100 kg/m³ (com1DFA's rhoEnt). The rest
   below one particle waits in `column_stats.eroded`. At most 64 per column and substep.
   - A new particle lies at rest on the ground of the column's cell, written with negative
     mass, so this substep's G2P does not gather a velocity for it: G2P only flips the sign.
   - The next P2G adds its mass without momentum. The flow slows by exactly the momentum the
     snow takes up: com1DFA's −u ρ dV/dt term, from the grid alone, with no extra force.
4. **When the pool is full**, the rest of the eroded snow goes back to the cover and
   `state.pool_exhausted` counts it.

All particles keep the same mass, so every existing count and sum stays a plain count:
averages run over release and entrained particles alike.

## Files

| File | Role |
|---|---|
| `mpm-mls-doc/avaframe/prepare_dataset.py`, `apps/webgpu_app/avalanche/AvaFrameData.cpp` | Rasterise the ENT shapefile like a release scenario: `entrainment.asc` (slope-normal thickness, 0.3 m where a feature has none) and `"entrainment"` in `dataset.json`. The two produce byte-identical output. |
| `webgpu/compute/nodes/AvaFrameSandboxNode.h/.cpp` | `SandboxTerrain::entrainment_*`: vertical depth (thickness / cos(slope)) without the release cells, bounds, volume; uploaded as `entrainment_texture`. |
| `webgpu/compute/nodes/AvaFrameReleaseNode.h/.cpp` | `resample_depth()` puts the cover onto the map region like the release; `ReleaseArea::entrainment_*`. |
| `webgpu/compute/nodes/MpmSolverNode.h/.cpp` | Settings (`EntrainmentMode`, densities, e_b, thresholds, uniform cover, pool size), `plan_cover()` (source and pool size at each reset), uniform fields, readback (`cover_volume`, `entrained_*`, `pool_exhausted_volume`). |
| `webgpu/compute/shaders/mpm_common.wgsl` | Uniform fields, `ENTRAINMENT_*` / `COVER_*`, `SimState` counters, `ColumnStats.cover` / `.eroded`, binding 12 (cover texture), `cover_depth_at()`, `under_release()`. |
| `webgpu/compute/shaders/mpm_prepare.wgsl` | The cover per column at the reset. |
| `webgpu/compute/shaders/mpm_grid_update.wgsl` | `entrain_column()`: erosion or plowing, particles from the pool. |
| `webgpu/compute/shaders/mpm_g2p.wgsl` | Turns a new particle's negative mass positive, at rest. |
| `webgpu/compute/shaders/mpm_seed.wgsl` | Leaves the pool slots inactive. |
| `apps/webgpu_app/avalanche/AvalanchePanel.cpp` | Entrainment controls and readout (`draw_entrainment()`), the mode per context (`sync_entrainment()`), the export block `"entrainment"` in `params.json`. |
| `mpm-mls-doc/avaframe/run_com1dfa.py` | com1DFA 2.1 on a dataset in three modes (null, erosion, plowing), as the reference. |
| `mpm-mls-doc/avaframe/compare_aimec.py` | `--reference null|erosion|plowing|auto` rates an MPM run against those runs. |
| `mpm-mls-doc/avaframe/batch_entrainment.json` | The MPM side: five avalanches, three modes. |

## Panel controls (Avalanche panel, below Basal friction)

| Control | Setting | Default |
|---|---|---|
| Entrainment (Off / Plowing / Erosion) | `entrainment_mode`, kept per context | map Erosion, sandbox Off |
| Erosion energy (erosion) | `erosion_energy` | 5000 J/kg |
| Pick-up depth (plowing) | `plowing_depth` | 0.1 m |
| Minimum speed | `entrainment_min_speed` | 1 m/s |
| Cover thickness, Cover up to (generic map scenarios) | `cover_thickness`, `cover_max_slope` | 0.3 m, 50° |
| Cover density | `entrainment_density` | 100 kg/m³ |
| (graph editor) | `pool_size`, particles per release particle | 2 |

Switching entrainment on or off, and changes to the cover, reseed. The readout shows the
cover in the domain, the entrained volume (as cover) and mass, and a warning when the pool
ran out.

## Validation against com1DFA with entrainment (2026-10-27)

Report with chart: [Avalanche Entrainment](https://claude.ai/artifact/3j7VDeZNRUf3mF3sE9TpNc) (claude.ai, private).

**Setup.** com1DFA 2.1 computed locally (`avaframe/run_com1dfa.py`) on the five avalanches in
three modes (null, erosion, plowing), with the null-test benchmarks' settings (samosAT,
ρ 200 kg/m³) and ρ_cover 100 kg/m³. MPM: Cam Clay, Li case V, Coulomb, whole DEM, 512² grid,
65 536 release particles, same three modes (`avaframe/batch_entrainment.json`). AIMEC, runout
at ppr > 1 kPa, against the com1DFA run of the same mode (`compare_aimec.py --reference auto`)
and against the old null benchmark.

**Off changes nothing:** the MPM runs without entrainment give the 11 Oct runout differences
to the decimetre on all five; com1DFA 2.1 null reproduces the 1.11 benchmarks' runout.

Entrained volume [m³, as cover]:

| Avalanche | Cover [m³] | MPM erosion | com1DFA erosion | MPM plowing | com1DFA plowing |
|---|---|---|---|---|---|
| Kontertal | 2 660 | 1 434 | 1 788 | 2 394 | 2 210 |
| Gallreide | 58 309 | 31 318 | 37 974 | 50 513 | 80 963 |
| Hintertux | 258 575 | 155 585 | 191 604 | 208 764 | 317 480 |
| Wolfsgruben | 114 621 | 63 548 | 76 307 | 93 219 | 114 445 |
| Madlein | 369 607 | 107 488 | 194 050 | 226 883 | 396 114 |

- Erosion: the MPM takes 80–83 % of com1DFA's volume on four avalanches, 55 % on Madlein
  (where the MPM stops ~200 m short, also without entrainment).
- Plowing: com1DFA takes more than the cover holds (Hintertux 123 %, Gallreide 139 %); its
  source says so (`DFAfunctionsCython.pyx`: every particle in a cell takes the cell's plowing
  mass in the same step, the cell is clamped afterwards). The MPM takes each column once:
  61–90 % of the cover.

Runout difference to com1DFA [m, along the AIMEC path]:

| Avalanche | MPM − com1DFA: off | erosion | plowing | erosion vs null benchmark | plowing vs null benchmark |
|---|---|---|---|---|---|
| Kontertal | +213 | +227 | +228 | +223 | +228 |
| Gallreide | +158 | +105 | +136 | +120 | +181 |
| Hintertux | +62 | +33 | +48 | +48 | +67 |
| Wolfsgruben | +84 | +59 | -118 | +74 | -94 |
| Madlein | -226 | -170 | -212 | -148 | -177 |

- Mean |difference| to com1DFA of the same mode: 149 m off, 119 m erosion, 148 m plowing.
  Against the old null benchmark: 123 m erosion, 149 m plowing.
- Entrainment moves com1DFA's runout by −4 to +45 m; the MPM's by −177 to +78 m, both ways.
- Peak speed drops with entrainment on every avalanche (Gallreide 126 → 117 → 94 m/s); the
  MPM stays about twice as fast as com1DFA. Time to rest unchanged within 2 s.
- Pool never ran out (2× the release particles).

**Which is more accurate?** With erosion, slightly, not decisively (closer to com1DFA; there
are no measured runouts). Erosion lowers the mean runout error from 149 to 119 m (four of
five improve, Kontertal slightly worse) and improves the runout-area precision on three of
five; plowing gives nothing on average. The MPM's main error is its speed (about twice
com1DFA's, ~150 m of runout), which entrainment does not fix, and its effect points both ways.
So: erosion as the default wherever entrainment is on, the sandbox comparisons kept with and
without, and entrainment not presented as what makes the MPM accurate.

**On the map** (native build): Hintertux cover 258 413 m³ (dataset 258 531), erosion took
189 601 m³; Breite Ries (generic, 0.3 m up to 50°) 5.3 M m³ of cover, 82 960 m³ taken
(~46 000 pool particles).

## Limits

- **The cover is per grid column.** Its outline is resolved at dx (5 m in the sandbox, 8–12.5 m
  on the map), not at the 5 m of AvaFrame's raster.
- **Entrained snow starts as release snow:** same material state and density as the release.
  com1DFA does the same (its entrained mass joins the flow); a weaker cover material is not
  modelled.
- **Not resolved at this resolution:** erosion versus entrainment, basal abrasion and
  erosion–deposition waves (Li et al. 2022, 2024). They need a cover 1–2 cells thick at
  0.2–0.7 m cells.
- **No deposition back into the cover** (com1DFA's detrainment is off by default too).
