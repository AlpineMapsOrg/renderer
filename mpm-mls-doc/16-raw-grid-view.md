# 16 — Raw grid data for visualisation

The solver hands out its grid velocity, with everything needed to read it, for code that builds
something from the flow field: a visualisation with animated streamlines, an export. The data is
raw: the solver's own terrain-following grid, not a finished 2D field. What a reader makes of it
(a depth average, another weighting, a finer grid) is the reader's. The solver only guarantees
that the grid is trustworthy to read as a whole and describes its layout.

The entry point is `MpmSolverNode::raw_view()`, which returns an `MpmRawView`
(`webgpu/compute/nodes/MpmSolverNode.h`).

## What is handed out

| Field | Meaning |
|---|---|
| `particles` | The solver's particle view, which the 3D renderer also reads: the particle buffers, the grid geometry (`domain_origin`, `dx`, `columns`), the placement on the map (`world_from_metres`), the terrain texture, `reset_count` |
| `grid_velocity` | One `vec4f` per grid node: `xyz` the velocity [m/s], `w` the node's mass in units of the normalised particle mass. Zero for nodes without flow. `nullptr` before the first run |
| `column_floor` | Per column, the first stored layer in cells above the origin (`i32`) |
| `grid_resolution` | Columns in x and y, layers per column |
| `kg_per_mass_unit`, `snow_density` | Physical mass of one unit of node mass [kg], and the density [kg/m³]. `kg_per_mass_unit` is 0 until the first diagnostics readback of a run has told the real volume of a particle |
| `interpolation` | Real-time playback: 0–1 from the particles before the last chunk (`particles->previous_particles`) to the ones after it. The grid keeps no previous state |
| `simulated_time` | Simulated time since the last reset [s] |
| `generation` | Counts the chunks that advanced the simulation; changes whenever `grid_velocity` does |
| `resource_generation` | Changes when the solver reallocates its buffers: rebuild bind groups |
| `layout_version` | `MPM_LAYOUT_VERSION`: bumped whenever the layout of the particles, the column stats, the grid velocity or the column floors changes. `MpmParticleView` carries it too |

The velocity is in the solver's metre frame, with z up; the linear part of
`particles->world_from_metres` turns its direction into the map's.

## The grid layout

The grid is three-dimensional, stored in a band of layers that follows the terrain
([04-data-layout.md](04-data-layout.md), terrain-following grid), layer-major. The node of column
`c = y · columns.x + x` in layer `l` is

```
grid_velocity[l · columns.x · columns.y + c]
```

and its position is `(domain_origin + (x, y) · dx, (column_floor[c] + l) · dx)`. Nodes of a column
are `columns.x · columns.y` apart, and the layers of a column start just below its terrain height,
so a layer is not an altitude. A depth average over a column is a loop over `l` with a stride, and
needs nothing but `grid_resolution`.

## What the solver guarantees

Three changes in the solver make the grid readable as a whole:

- **The node mass is in `w`.** The grid update writes it into the fourth component, which was
  unused. It marks empty nodes and gives mass-weighted averages and the flow depth.
- **Tiles the flow has left are cleared once.** The grid update processes only the tiles that P2G
  flagged in the substep, so a tile the flow had left would keep its last velocity and mass. The
  tile flag has a second bit, `WAS_ACTIVE`, set when the grid update processes a tile; a tile with
  only that bit is processed once more, finds no mass and zeroes its nodes
  ([03-shaders.md](03-shaders.md)).
- **A reset clears `grid_velocity`,** because the flags are cleared with it and the old tiles would
  never be visited.

G2P reads only nodes in flagged tiles, so none of this changes the physics.

## What to know when reading it

- **Nodes at the edge of the flow have almost no mass but can have large velocities.** A node's
  momentum includes the stress impulse of the particles, so a node with very little mass can show
  hundreds of m/s that no particle has. Weight by `w`, and treat columns with very little total
  mass (less than a particle's worth) as empty.
- **The grid is rewritten every substep.** It reflects the last substep of the last submitted
  chunk, and has no previous state. For smooth playback keep a copy per chunk, which costs 16 B per
  node (18.8 MiB at 320² × 12), or interpolate a 2D field built from it.
- **Reading is cheap to arrange.** A reader binds the buffer read-only in its own bind group:
  nothing is copied and no synchronisation is needed, because WebGPU has one queue per device and
  the reader's pass runs after the solver's chunks. The solver's own bind group already uses all
  eight storage buffers a shader stage may have, so a reader cannot extend it.
- **Depth needs the particle volume.** The real volume of a particle is known after the first
  diagnostics readback (`kg_per_mass_unit`); depth is `w`-sum · `kg_per_mass_unit` /
  (`snow_density` · `dx²`).
- **The layout is versioned.** It changes with solver optimisations (a fused G2P2G would need a
  double-buffered grid); `layout_version` tells a reader when to update. The renderer's copy of the
  layout is guarded at compile time ([04-data-layout.md](04-data-layout.md), layout version).

## Verification

`check_substep.mjs` ([bench/README.md](bench/README.md)) reads the grid velocity buffer back after
the whole-kernel test ([06-verification.md](06-verification.md) §4q): the node mass in `w` sums to
the number of particles (300.0000 for 300); after the flow has been moved 500 m the sum is still 300,
not 600 (the check fails with the clearing switched off); nodes without mass hold no velocity. The
whole-kernel errors against the float64 reference are the same as before the change.

Not covered: a reader of the raw view in a running app (none exists yet), and a resize of the
solver's grid while a reader is attached.

## Cost

The mass write and the clearing are always on in the solver. Three A/B runs against the previous
kernels at 131 072 particles on an Apple M5 gave 0.96–1.05× on the GPU time per run, with an
identical centre-of-mass trace; the grid update stage was 0.51–0.64 ms against 0.54–0.56 ms
([09-performance-analysis.md](09-performance-analysis.md) §3.5). The benchmark's flow moves only
34 m, so few tiles are left behind; a long run-out leaves more, which has not been measured. A reset
clears the grid velocity once. Handing out the view costs nothing; a reader's own work is its own.
