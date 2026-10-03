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
| [07-constitutive-models.md](07-constitutive-models.md) | Proposal v3.0 gap analysis: which material/friction models exist, and the design for making them exchangeable. |
| [report-implementation-audit.md](report-implementation-audit.md) | Issue #3 report: paper-to-code comparison, deviations, verification boundaries, and recommended next steps. |
| [08-domain-size-options.md](08-domain-size-options.md) | Why the domain was 1.6 km, the options for a whole avalanche path, measured GPU cost, and what was chosen (terrain-following grid). |
| [09-performance-analysis.md](09-performance-analysis.md) | **Performance**: why the demo felt slow, where the GPU time goes, VRAM and device limits, leaks found and fixed, main-loop/threading options, the benchmark method, and the first round of optimisations with measurements. |
| [bench/](bench/README.md) | Standalone WebGPU benchmark of the real kernels, headless A/B against any git revision. |
| [10-benchmark-results.md](10-benchmark-results.md) | Every checked-in benchmark result as a table: Apple M5 (Chrome/Metal) and SwiftShader, per material, per stage. Generated from `bench/results/*.json`. |
| [report-fixed-point-energy-pump.md](report-fixed-point-energy-pump.md) | Standalone write-up of the fixed-point blow-up investigation. |
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
state, which is what makes it animate. Each step is the classic MPM loop (the grid clear is
folded into the grid update since 2026-09-27):

```
P2G (flags active tiles) → grid update (active tiles only, clears as it goes) → G2P + advection
```

All of it, plus the visualisation passes, runs in **one WebGPU compute pass per submitted
chunk** (a few substeps per frame, paced from the measured GPU time).

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
- Material layer is **done** as far as proposal v3.0 goes (2026-09-13): Stomakhin,
  Drucker–Prager and Cohesive Cam Clay switchable at runtime, Coulomb and Voellmy basal
  friction, seven presets from Li et al. 2021, a per-run diagnostics readback, and the
  com1DFA energy-line test — which recovers the set basal friction to under 1 % offline
  and runs live in the sidebar. What remains there is entrainment —
  [07-constitutive-models.md](07-constitutive-models.md).
- **No CK-MPM** [6]. Would touch only P2G/G2P transfer, per the stretch goal.
- Not validated against any real avalanche. Educational only — as the proposal states.
