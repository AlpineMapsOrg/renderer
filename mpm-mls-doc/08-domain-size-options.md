# Simulation domain: why it is small, and the options for making it big enough

*Analysis 2026-09-13. **E + B were implemented the same day** — see 04-data-layout.md "The terrain-following grid" and 06 §4i. The other options stay here as the record of what was considered.*

## 1. What the code does today

Two nested boxes, both settings of the graph preset (`mpm_avalanche_simulation.json`) and
of the scenario table in `AvalanchePanel`:

| Box | Set by | Preset value | Limit in code |
|---|---|---|---|
| **Region** — the terrain that is downloaded, stitched, decoded, normal-mapped | `GeoRegionNode.extent`, `SelectTilesNode.zoomlevel` | 2500 m, zoom 15 (12.8 m/px, 834 m tiles) | stitched image ≤ 8192 px (`TileStitchNode`) → 105 km at z15, 26 km at z17 |
| **Domain** — the MPM grid box | `MpmSolverNode.domain_size_xy`, `grid_resolution_xy/z` | 1600 m, 128×128×128, `dx` = 12.5 m | `MAX_GRID_RESOLUTION` = 256 per axis; domain clamped into the region (`update_gpu_settings`) |

The grid is a **dense 3D array**, `res_x · res_y · res_z` nodes × 20 B, cleared and
updated in full every substep:

- 128³ = 2.1 M nodes = 42 MB; clear + grid update touch all of them 24× per run.
- The snow occupies 2–3 layers above the terrain at any (x, y). Terrain relief of 600 m
  in the preset spans ~50 layers, the remaining layers are air. **~98 % of the nodes never
  carry mass.** Grid passes are ~40 % of the per-substep work (2 × 2.1 M node-ops vs
  2 × 3.5 M stencil-ops for 131 k particles).
- Domain size scales as `res³`: doubling the edge at constant `dx` costs 8× memory and 8×
  grid work. That is the actual reason the domain is 1.6 km.

Boundary behaviour, which is what "clamped" looks like on screen:

- `mpm_grid_update`: nodes with index < 2 or ≥ res − 3 get their outward velocity zeroed
  (all six faces).
- `mpm_g2p`: particle positions clamped to `[2 dx, size − 2 dx]` in x, y and to the
  vertical range, with that velocity component zeroed.

So the domain faces are frictionless walls. Snow reaching one piles up against it like a
dam and the runout is cut off — physically meaningless past that point.

Two facts that make the options below cheap:

1. **The grid holds no state between substeps.** It is rebuilt from the particles every
   substep (clear → P2G). The domain origin, size and `dx` can change between runs
   without any transfer; only the vertical origin scan (`mpm_prepare`) has to be re-run.
2. **Downstream follows automatically.** `AvalanchePanel` steps the solver with
   `rerun()`, `NodeGraph` chains `run_completed` to the next node, and
   `OverlayRenderNode` re-reads the `domain aabb` output every run. A moving or growing
   domain needs no change outside the solver.

## 2. Options

Estimates count lines outside `MpmSolverNode.*`, the `mpm_*.wgsl` shaders, the two
panels we own (`AvalanchePanel`, `MpmSolverNodeRenderer`) and our graph preset. Those are
the lines a maintainer of the upstream repo has to review.

### A. Bigger dense grid (settings only)

Raise `domain_size_xy`, `grid_resolution_xy` (to 256) and reduce `grid_resolution_z` to
what the relief needs.

| | |
|---|---|
| Reach | 3.2 km at 12.5 m (256 × 256 × 64 = 4.2 M nodes, 84 MB, 2× grid work); 5 km at 20 m |
| Non-MPM code | **0 lines** (preset JSON, scenario table) |
| MPM code | ~10 lines: allow `MAX_GRID_RESOLUTION` 512 in xy, keep z separate (already is) |
| Downside | Still `res³`; 5 km at 12.5 m would be 400 × 400 × 64 = 10 M nodes and ~5× today's grid cost. Quality or size, not both. |

### B. Terrain-following grid ("column band") — the real fix

Keep only `K` layers per (x, y) column, starting a fixed number of cells below the
terrain: node `(i, j, k)` lives at `z = floor(i, j) + k · dx`, with
`floor(i, j) = ⌊terrain(i, j) / dx⌋ · dx − 2 dx` written once per reset by `mpm_prepare`
into a `column_floor[res_x · res_y]` buffer. A stencil node in a neighbouring column is
mapped through that column's own floor; if it falls outside `[0, K)` it does not exist
and is skipped (that only happens ≥ 2 cells inside the terrain or > (K − 3) cells above
it, where nothing carries mass anyway).

| | |
|---|---|
| Reach | 512 × 512 × 12 = 3.1 M nodes = 63 MB → **6.4 km at 12.5 m**, or 3.2 km at 6.25 m. Grid work +50 % vs today for 16× the area. |
| Non-MPM code | **0 lines** |
| MPM code | ~150 lines: `grid_index` / `is_inside_grid` / `to_grid_space` in `mpm_common`, one buffer + binding, `mpm_prepare` writes floors, clear/update dispatch over `res_x · res_y · K`, a `layers` setting in the node and both panels. |
| Constraint | Adjacent columns must not differ by more than ~K − 4 layers: with K = 16 at 12.5 m that is a 150 m step between neighbouring cells — no Alpine DTM at this resolution does that. K is a setting; the prepare pass can count violations into `SimState` for a warning. |
| Physics change | None. Nodes deep inside the terrain today only get their velocity projected; particles never reach them (measured: all within one cell of the surface). Dropping them from the stencil is a partition-of-unity loss only at cliffs steeper than the constraint. |

This is what turns the size problem from `res³` into `res²`: memory and grid work then
scale with the DEM footprint, like a thickness-integrated model's, while the solver
stays fully 3D within the band.

### C. Sparse / blocked grid (SPGrid-style)

Activate 4³ or 8³ blocks where particles are, hash them, run passes over active blocks
only. Truly unbounded, but WGSL has no dynamic allocation: it needs a block-activation
pass, a hash table with collision handling in atomics, a compaction pass, and a
second-level index in every P2G/G2P access. Hundreds of lines in the hot loop, the
lookup makes P2G slower, and it debugs badly. Reach beyond B is not needed for a
single avalanche path (a 6 km box holds every runout in the Alps). **Not recommended.**

### D. Moving / growing window (on top of A or B)

Because the grid holds no state, the domain can follow the snow: after each run take the
particle bounding box (four `atomicMin/Max` in `mpm_splat`, read back with the state
that already comes back), set the next run's domain to that box plus a margin, clamped to
the region, and `dx = max(size / res, dx_min)`. Re-run `mpm_prepare` when the origin
moves (it is a `res_x · res_y` dispatch, negligible).

| | |
|---|---|
| Reach | Whatever the region holds, with the grid concentrated on the snow; resolution degrades as the deposit tail and the front spread apart. |
| Non-MPM code | **0 lines** — the overlay re-reads the aabb each run, the height and release textures are region-wide already. |
| MPM code | ~80 lines in `MpmSolverNode` (bbox readback, origin/`dx` update, prepare on move) + 4 counters in `mpm_splat`. |
| Downside | Visible re-rasterisation when the box jumps; the release area gets coarser late in the run. Without B it only postpones the `res³` problem. |

### E. Larger terrain region (prerequisite for all of the above)

The domain can never leave the region. `GeoRegionNode.extent` 2500 → 8000 m keeps a
6 km domain plus margin covered. At z15 that is 100 tiles, a 625 px stitched texture —
trivial; z16 (6.4 m/px, 1250 px) if `dx` goes below 10 m.

| | |
|---|---|
| Non-MPM code | **0 lines**: `extent` and `zoomlevel` are existing settings; the scenario table in `AvalanchePanel` carries them per location already. |
| Note | `ComputeReleasePointsNode` and `ComputeNormalsNode` scale with the texture; `sampling_interval` may need a bump for big regions or every slope becomes a release point. |

## 3. Recommendation

**E + B**, then D if a single scenario ever needs more than ~6 km: region to 8 km,
terrain-following grid with 12–16 layers, domain 4–6 km at 12.5 m. Zero lines outside
the code we already own; the maintainers see the same file set as today. A dense-grid
stopgap (A) is one JSON edit if something is needed for a demo before B lands.

What B does *not* solve, for honesty in the report: `dx` stays ≥ ~10 m for real-scale
runs, so the slab is still sub-cell (see 05-tuning.md, "the resolution problem"). A
finer grid would need C or a multi-resolution scheme, neither of which is in scope.

## 4. Upstream footprint so far, for reference

Everything the project has touched outside its own files, per `git diff e459373c`:

| File | Lines | What |
|---|---|---|
| `webgpu/compute/NodeRegistry.cpp` | +4 | register the two nodes |
| `webgpu/compute/CMakeLists.txt` | +17 | add sources and shaders |
| `apps/webgpu_app/compute/nodes/NodeRendererFactory.cpp` | +4 | renderer for the node |
| `apps/webgpu_app/compute/NodeGraphPanel.{h,cpp}` | +5 | `node_graph()` accessor the panel uses |
| `apps/webgpu_app/ImGuiManager.cpp` | +4 | register the panel |
| `apps/webgpu_app/CMakeLists.txt`, `resources.qrc` | +3 | panel sources, graph preset |

None of the options adds to this list.

## 5. Measured cost (Apple M5, 10-core GPU, 2026-09-13)

Mean wall time per run (24 substeps of dt = 0.01 s, i.e. 0.24 s of simulated time), runs
11–50 of a fresh seed, Stomakhin preset. Configurations with 16 layers on the dense grid
clamp the particles vertically (200 m range against 600 m of relief), so their physics is
garbage; the timing is still representative of the grid passes, which is what they
isolate.

| Grid | Nodes | Particles | ms / run |
|---|---|---|---|
| 128 × 128 × 128 (preset) | 2.10 M | 131 k | **116** |
| 128 × 128 × 16 | 0.26 M | 131 k | 66 |
| 256 × 256 × 64 (option A, 3.2 km) | 4.19 M | 131 k | 162 |
| 256 × 256 × 16 | 1.05 M | 131 k | 83 |
| 512 × 512 × 16 (option B at 6.4 km) | 4.19 M | 131 k | 157 |
| 128 × 128 × 128 | 2.10 M | 262 k | 166 |
| 256 × 256 × 16 | 1.05 M | 524 k | 216 |

A linear fit over these is good to ~10 %:

```
ms per run ≈ 9  +  25 · (grid nodes / 10⁶)  +  0.38 · (particles / 10³)
per substep:      1.0 ms per 10⁶ nodes,      16 µs per 10³ particles
```

So today the grid passes (clear + update, 2 × 2.1 M nodes) are ~45 % of a run and the
particle passes (P2G + G2P) ~45 %; the rest is splat, rasterise and callback latency.

**What that means for the options.** "Real time" here is a run finishing within the
0.24 s it simulates; the preset runs 2× faster than that.

- **A** (256² × 64): 162 ms — still real time, at 2× the area. Its ceiling is 400² × 64
  ≈ 10 M nodes ≈ 260 ms — no longer real time, for 5 km.
- **B** (512² × 16 ≈ 4.2 M nodes): ~157 ms — real time at **4× today's edge, 16× the
  area**, same `dx`. 12 layers: ~130 ms. At 256² × 16 (3.2 km) it is *faster* than the
  preset is now, 83 ms. Memory 84 MB at 512² × 16.
- **D**: no per-run cost; the extra prepare dispatch is a `res_x · res_y` pass, well
  under a millisecond.
- **Particles are the other axis.** 0.38 ms per 1 000 particles per run puts the
  real-time ceiling at ~550 k particles regardless of grid; 262 k already costs as much
  as the whole grid does. Beyond that the options are fewer substeps per run (the panel
  then animates in larger simulated steps), a larger `dt` where CFL allows, or accepting
  slower than real time. The P2G atomics are the hot loop (03-shaders.md); a
  shared-memory P2G would be the next optimisation, but that is unrelated to the domain
  question.

Caveats: the timer wraps the whole run including the async readback callback, so it
carries up to one event-loop tick of latency (a few ms); the 16-layer dense-grid rows
have all particles piled at the vertical clamp, which changes atomic contention in P2G
somewhat. Neither changes the picture: B buys the area for roughly the cost of the
preset's own wasted air.
