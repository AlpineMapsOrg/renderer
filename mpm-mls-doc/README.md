# MLS-MPM avalanche simulation — implementation notes

Working notes on the MLS-MPM snow solver added to weBIGeo for the VisComp WS26 project.
Written to be read by me later: enough detail to pick the code back up, change parameters
with intent, and pull material into the final report.

Not a paper. Compact, technical, opinionated. Where the implementation deviates from the
literature, that is called out explicitly rather than smoothed over.

**On citations:** these notes *cite* the papers and point at the specific section or
equation each piece follows — they don't reproduce text from them. Pull the actual wording
from the sources when writing the report. Full list in [refs.md](refs.md).

## Read in this order

| File | What's in it |
|---|---|
| [01-theory.md](01-theory.md) | FLIP → MPM → MLS-MPM → snow model. The maths that is actually implemented, with pointers into the papers. |
| [02-files-and-api.md](02-files-and-api.md) | Every file added or touched, the C++ classes, what each method does. |
| [03-shaders.md](03-shaders.md) | The eight WGSL kernels, one at a time, with the formula each implements. |
| [04-data-layout.md](04-data-layout.md) | Buffer/struct layouts, fixed-point atomics, coordinate conventions, uniform layout rules. |
| [05-tuning.md](05-tuning.md) | Which knob does what, sensible ranges, failure modes, how to extend. |
| [06-verification.md](06-verification.md) | What was actually tested, how, and what is still unverified. |
| [refs.md](refs.md) | Bibliography. |

## 60-second orientation

The solver is a **compute node** in weBIGeo's existing node graph, so it reuses the terrain
front end (tile selection → stitch → height decode → normals → release points) untouched
and only replaces the simulation stage. This mirrors the structure of the existing
statistical trajectory model rather than competing with it — see Komon et al. [10] for the
compute-overlay architecture it plugs into.

```
Region ─→ Select Tiles ─→ Request Height ─→ Stitch Tiles ─→ Height Decode ─┬─→ Normals ─→ Release Points ─┐
                                                                           │                              │
                                                                           └──────────────┬───────────────┘
                                                                                          ↓
                                                                                     MPM Solver ─→ Overlay
```

One node execution = `substeps_per_run` MPM steps. Re-running continues from the current
state, which is what makes it animate. Each step is the classic four-stage loop:

```
clear grid → P2G → grid update → G2P + advection
```

All of it, plus the visualisation passes, runs in a **single WebGPU compute pass**.

## Current status

Works end to end: real terrain, real DEM, animated, positioned by latitude/longitude, with
a picker for stored locations.

Known gaps, honestly:

- **Output is a 2D top-down density raster**, not 3D particles. The proposal's Phase 5
  (a real `AvalancheRenderer` in `webgpu/engine/`) is not implemented. The node already
  exposes a raw `particle buffer` socket so a renderer can bind it without CPU readback.
- **Parameters are Stomakhin's paper values**, authored for metre-scale snow. At 12–16 m
  grid cells the released slab is sub-cell and the flow reads more fluid than slab-like.
  See [05-tuning.md](05-tuning.md).
- **No CK-MPM** [6]. Would touch only P2G/G2P transfer, per the stretch goal.
- Not validated against any real avalanche. Educational only — as the proposal states.
