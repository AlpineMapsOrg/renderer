# Simulation domain: the terrain-following grid

The solver's grid covers a whole avalanche path: the graph preset simulates a 4 km box at
12.5 m cells (320² columns), and the grid goes up to 512² columns, 6.4 km at the same cell size.
This is implemented as a **terrain-following grid**: each column stores only a band of node
layers that starts just below the terrain, so memory and grid work scale with the area, not
with the volume. This is due to the limitation of a dense 3D grid, whose cost grows with the
cube of the edge length (§3). The layout is in
[04-data-layout.md](04-data-layout.md#the-terrain-following-grid-2026-09-13), the verification in
[06-verification.md](06-verification.md) §4i.

## 1. What is implemented

**Region and domain.** Two nested boxes, both set in the graph preset
(`mpm_avalanche_simulation.json`) and per scenario in `AvalanchePanel`:

| Box | Set by | Preset | Limit |
|---|---|---|---|
| **Region**: the terrain that is downloaded, stitched, decoded and normal-mapped | `GeoRegionNode.extent`, `SelectTilesNode.zoomlevel` | 8000 m, zoom 15 (12.8 m/px) | stitched image ≤ 8192 px (`TileStitchNode`): 105 km at z15 |
| **Domain**: the MPM grid | `MpmSolverNode.domain_size_xy`, `grid_resolution_xy`, `grid_layers` | 4000 m, 320² × 12, `dx` = 12.5 m | 512 columns per side, 64 layers; the domain is clamped into the region (`update_gpu_settings`) |

At zoom 15 an 8 km region is a 625 px stitched texture, which costs next to nothing; zoom 16
(6.4 m/px) suits cells below 10 m.

**The band.** Column `(i, j)` stores `L` layers from
`floor(i, j) = ⌊terrain(i, j) / dx⌋ · dx − 2 dx`, which `mpm_prepare` writes once per reset into
`column_floor`. A stencil node in a neighbouring column is mapped through that column's own
floor; if it falls outside `[0, L)` it does not exist and is skipped. That only happens two or
more cells inside the terrain or near the top of the band, where nothing carries mass. Particles
are kept below `(column_floor + L − 2.5) · dx`, so their stencil stays inside the band: the pile headroom above the ground is
`dx × (L − 4.5)`, about 95 m with 12 layers at 12.5 m.

**Grid work follows the snow.** On top of the band, the grid update runs only over the 8 × 8-
column tiles that P2G touched ([09-performance-analysis.md](09-performance-analysis.md) §2.1).
The grid holds no state between substeps, it is rebuilt from the particles every substep.

**Box boundaries.** The sides of the box and the top of the band are frictionless walls: the
grid update zeroes the outward velocity on the two outer node rows and the top three layers,
and G2P clamps particle positions to `[2 dx, size − 2 dx]`. Snow reaching a wall piles up
against it and its runout is cut off, so G2P counts particle-substeps whose stencil reaches
those nodes, and the sidebar warns with their share.

## 2. Limits

- **Resolution.** `dx` stays at about 10 m or more for real-scale runs, so the released slab
  is thinner than a cell ([05-tuning.md](05-tuning.md), "the resolution problem"). A finer grid
  over the same area would need a sparse or multi-resolution grid (§4).
- **Terrain steps.** Neighbouring columns must not differ by more than about `L − 4` layers,
  otherwise a stencil loses nodes: with 12 layers at 12.5 m that is a 100 m step between
  adjacent cells, which no Alpine terrain model at this resolution has.
- **Region.** The domain never leaves the downloaded region; a larger domain needs a larger
  `GeoRegionNode.extent`.

## 3. Why not a dense grid

A dense grid stores `res_x · res_y · res_z` nodes. The snow occupies 2–3 layers above the terrain
at any point, while 600 m of relief spans about 50 layers at 12.5 m, so about 98 % of a dense
grid's nodes never carry mass, and doubling the edge at constant `dx` costs 8× the memory and
the grid work. A dense 128³ grid (42 MB) limits the domain to 1.6 km at 12.5 m.

Measured on an Apple M5 (2026-09-13, the kernels of that time, which cleared and updated every
node every substep): mean wall time per run of 24 substeps at dt 0.01 s, runs 11–50 after a
fresh seed, Stomakhin. The 16-layer dense rows clamp the particles vertically, so their physics
is meaningless; their timing still isolates the grid passes.

| Grid | Nodes | Particles | ms per run |
|---|---|---|---|
| 128 × 128 × 128, dense, 1.6 km | 2.10 M | 131 k | **116** |
| 128 × 128 × 16 | 0.26 M | 131 k | 66 |
| 256 × 256 × 64, dense, 3.2 km | 4.19 M | 131 k | 162 |
| 256 × 256 × 16 | 1.05 M | 131 k | 83 |
| 512 × 512 × 16, band, 6.4 km | 4.19 M | 131 k | 157 |
| 128 × 128 × 128 | 2.10 M | 262 k | 166 |
| 256 × 256 × 16 | 1.05 M | 524 k | 216 |

A linear fit is good to about 10 %:

```
ms per run ≈ 9  +  25 · (grid nodes / 10⁶)  +  0.38 · (particles / 10³)
```

So the band covers 16× the area of the dense 1.6 km grid at the same `dx` for about 1.4× its
cost, while a dense grid at 5 km (400² × 64, 10 M nodes) would take about 260 ms per run, slower
than the 0.24 s it simulates. The fit times the whole run including the readback callback, so
it carries a few milliseconds of event-loop latency. The current kernels are 4–8× faster than
these numbers ([09-performance-analysis.md](09-performance-analysis.md)); the comparison between
dense and band grids is unchanged by that.

## 4. Alternatives not chosen

- **A bigger dense grid** (256² × 64, 3.2 km): settings only, but still cubic, so size and
  resolution trade against each other.
- **A sparse, blocked grid** (SPGrid-style: active 4³ or 8³ blocks found through a hash): truly
  unbounded, but WGSL has no dynamic allocation, so it needs a block-activation pass, an atomic
  hash table with collision handling, a compaction pass and a second-level index in every P2G
  and G2P access. That is hundreds of lines in the hot loop and a slower P2G, for a reach no
  single avalanche path needs: a 6 km box holds every runout in the Alps.
- **A moving or growing window** that follows the particles' bounding box: possible, because
  the grid holds no state, and cheap (a bounding-box readback and a `mpm_prepare` dispatch per
  move), but the resolution coarsens as the front and the deposit tail spread apart, and the
  overlay jumps when the box moves. With the band grid it is not needed below about 6 km.
