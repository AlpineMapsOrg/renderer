# MLS-MPM avalanche simulation in weBIGeo

Technical documentation of the MLS-MPM snow solver that the VisComp WS26 project added to
weBIGeo. It explains how the solver works, how it is built into weBIGeo, how it was verified
and how fast it is, for anyone who wants to understand the project, run it or change it.

Where the implementation deviates from the literature, the documents say so and give the
reason.

> **AI assistance.** The implementation and this documentation were written with Claude
> (Anthropic's AI model, used through Claude Code), directed by the project author. Statements
> about what the code does and how well it works are backed by the tests and measurements in
> [06-verification.md](06-verification.md) and [09-performance-analysis.md](09-performance-analysis.md);
> paper references were checked against the PDFs. The project author also inspected the shader
> files and the C++ files manually.

**Citations:** the documents cite the papers by section and equation, without reproducing
their text. Full list in [refs.md](refs.md); the verification of every formula against its
source is the separate shader verification report (see [06-verification.md](06-verification.md)).

## What this project adds to weBIGeo

weBIGeo already had a statistical trajectory model for avalanches. This project adds:

- **A physical avalanche simulation.** An MLS-MPM snow solver on the GPU (`MpmSolverNode`),
  run on weBIGeo's own terrain: three exchangeable snow materials, Coulomb and Voellmy basal
  friction, entrainment of the snow cover, a terrain-following grid for whole avalanche paths
  ([01](01-theory.md)–[03](03-shaders.md), [07](07-constitutive-models.md),
  [08](08-domain-size-options.md), [15](15-entrainment.md)).
- **A simulation driver.** A generic `TimeSteppedSimulation` interface and a `SimulationDriver`
  that plays any time-stepped compute node back frame by frame: in real time at 0.1–4× with
  interpolation, or as fast as the GPU allows. MPM is the first node that uses it
  ([13](13-simulation-driver.md)).
- **3D avalanche rendering on the map.** An `AvalancheRenderer` in the engine that draws the
  flow into the G-buffer: a draped flow surface, the track, the snow cover, and the particles
  as powder, ellipsoids or spheres ([12](12-rendering.md)).
- **Switchable map overlays.** `Overlay::enabled` in the engine: a disabled overlay is left out
  of the overlay chain, so the 2D density overlay can hide while the 3D view is shown
  ([02](02-files-and-api.md)).
- **AvaFrame integration.** Downloads AvaFrame's real test avalanches in the app, runs them in a
  sandbox on AvaFrame's DEM or on the map from AvaFrame's release areas, and compares a
  simulation with com1DFA using AIMEC. The comparison scripts also rate weBIGeo's existing
  trajectory model, not only MPM ([11](11-avaframe-comparison.md),
  [14](14-avaframe-release-on-map.md), [avaframe/](avaframe/README.md)).
- **An avalanche panel and scenarios.** A sidebar panel with a location picker, playback and
  the main settings; ready-to-run graph presets; `GeoRegionNode`, which builds a region from
  latitude, longitude and extent, so a scenario needs no GPX file ([02](02-files-and-api.md)).
- **Benchmark and test tooling.** A standalone WebGPU benchmark of the real kernels, run
  headless against any git revision, plus GPU golden tests and a whole-kernel substep test
  ([bench/](bench/README.md), [06](06-verification.md), [09](09-performance-analysis.md)).

## Read in this order

| File | What's in it |
|---|---|
| [01-theory.md](01-theory.md) | FLIP → MPM → MLS-MPM → snow model. The maths that is actually implemented, with pointers into the papers. |
| [02-files-and-api.md](02-files-and-api.md) | Every file added or touched, the C++ classes, what each method does. |
| [03-shaders.md](03-shaders.md) | The twelve WGSL kernels, one at a time, with the formula each implements. |
| [04-data-layout.md](04-data-layout.md) | Buffer/struct layouts, fixed-point atomics, coordinate conventions, uniform layout rules. |
| [05-tuning.md](05-tuning.md) | Which knob does what, sensible ranges, failure modes, how to extend. |
| [06-verification.md](06-verification.md) | What was actually tested, how, and what is still unverified. |
| [07-constitutive-models.md](07-constitutive-models.md) | Proposal v3.0 gap analysis: which material/friction models exist, and the design for making them exchangeable. |
| [08-domain-size-options.md](08-domain-size-options.md) | Why the domain was 1.6 km, the options for a whole avalanche path, measured GPU cost, and what was chosen (terrain-following grid). |
| [09-performance-analysis.md](09-performance-analysis.md) | **Performance**: GPU time by particle count and material, where it goes and why the kernels are fast (active tiles, sorted P2G, one SVD), memory and device limits, sharing the GPU with the map, resource lifetime, the benchmark method, and what would make it faster. |
| [scripts/](scripts/README.md) | **Python scripts**: every script in this folder, what it checks or produces, setup and how to run it — shader validation, the Python ports, the GPU golden tests, the AvaFrame comparison, the benchmark tables. |
| [bench/](bench/README.md) | Standalone WebGPU benchmark of the real kernels, headless A/B against any git revision. |
| [10-benchmark-results.md](10-benchmark-results.md) | Every checked-in benchmark result as a table: Apple M5 (Chrome/Metal) and SwiftShader, per material, per stage. Generated from `bench/results/*.json`. |
| [report-fixed-point-energy-pump.md](report-fixed-point-energy-pump.md) | Standalone write-up of the fixed-point blow-up investigation. |
| [11-avaframe-comparison.md](11-avaframe-comparison.md) | **AvaFrame comparison**: MPM variants, com4FlowPy and the weBIGeo trajectory model against com1DFA on five real avalanches, rated with AIMEC. Setup, tables, findings, limits. |
| [12-rendering.md](12-rendering.md) | **3D rendering on the map**: what is implemented and where — particles (spheres, ellipsoids, detail particles, powder), the mass-splat flow surface, track and snow cover, the frame order, panel controls, limits. |
| [14-avaframe-release-on-map.md](14-avaframe-release-on-map.md) | **AvaFrame release areas on the map**: the five AvaFrame scenarios release from AvaFrame's areas (resampled through the affine placement), what is approximate, measurements. |
| [15-entrainment.md](15-entrainment.md) | **Entrainment**: the flow picks up the snow cover (com1DFA's plowing and erosion), cover per grid column from AvaFrame's entrainment areas or one thickness, eroded snow from a particle pool; files, panel, validation against com1DFA with entrainment. |
| [13-simulation-driver.md](13-simulation-driver.md) | **Playback**: the generic `TimeSteppedSimulation` interface and `SimulationDriver`, real time (fixed-step loop with interpolation, one chunk per frame, 0.1–4x) vs accuracy, with measurements. |
| [avaframe/](avaframe/README.md) | **AvaFrame comparison, phase 1**: prepares AvaFrame's test cases (DEM, release, com1DFA reference rasters, AIMEC path, map overlays) for comparing against the MPM solver. |
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
                                                                                     MPM Solver ─┬─→ Overlay       (2D density raster)
                                                                                                 └─→ Particles 3D  (AvalancheRenderer, map graph)
```

One node execution = `substeps_per_run` MPM steps. Re-running continues from the current
state, which is what makes it animate. Each step is the classic MPM loop; the particles are
sorted by grid tile once per submitted chunk, so that P2G can sum each tile's contributions in
workgroup memory ([03-shaders.md](03-shaders.md)):

```
sort particles by tile (once per chunk)
P2G (tile order, flags active tiles) → grid update (active tiles only, clears as it goes) → G2P + advection
```

All of it, plus the visualisation passes, runs in **one WebGPU compute pass per submitted
chunk** (a few substeps per frame, paced from the measured GPU time). Playback is the
`SimulationDriver`'s, every frame: real time on the map, as fast as the budget allows in
the sandbox — [13-simulation-driver.md](13-simulation-driver.md).

## Current state

**What works.** The solver runs end to end on real terrain in the native app and in the
browser: released at stored locations or at AvaFrame's release areas, animated in real time
or as fast as the GPU allows, drawn as a 2D density overlay or in 3D on the map.

- **Materials and friction** ([07-constitutive-models.md](07-constitutive-models.md)):
  Stomakhin, Drucker–Prager and Cohesive Cam Clay, switchable at runtime; Coulomb and Voellmy
  basal friction; seven presets from Li et al. 2021; a per-run diagnostics readback and
  com1DFA's energy-line test, which recovers the set basal friction to under 1 %.
- **Entrainment** ([15-entrainment.md](15-entrainment.md)): com1DFA's plowing and erosion, in
  the sandbox and on the map.
- **3D rendering** ([12-rendering.md](12-rendering.md)): a draped flow surface from a B-spline
  mass splat, a track, a snow cover, and the particles as powder, ellipsoids or spheres.
- **Comparison with AvaFrame** ([11-avaframe-comparison.md](11-avaframe-comparison.md)):
  five real avalanches against com1DFA, rated with AIMEC.

**Verification** ([06-verification.md](06-verification.md)). Every formula of the shaders was
compared with its source paper. Python ports of the shader functions are tested against
analytic properties of the models; GPU golden tests compare the real WGSL functions with the
ports; a whole-kernel test runs the sort, P2G, grid update and G2P on the GPU against a float64
reference. The deliberate deviations from the papers: the non-associated Cam Clay return of
Wolper et al. 2019 instead of the associative one Li's presets were calibrated with, no
friction-angle hardening for Drucker–Prager, and the resolution (Li et al.: 0.5 m cells, here
5–12.5 m).

**Performance** ([09-performance-analysis.md](09-performance-analysis.md),
[10-benchmark-results.md](10-benchmark-results.md)). On an Apple M5 the graph preset (65 536
particles, 320² × 12 grid) takes 6.4 ms of GPU time per 24 substeps, 131 072 particles take
12.5 ms and 1 M particles 106 ms. At the preset's time step of 0.02 s that is about 70× faster
than real time for the preset and 4.5× for 1 M particles, if the solver had the GPU to itself.

**Limitations.**

- **Resolution.** At 5–12.5 m grid cells the released slab (about 1 m) is thinner than a cell,
  so the material models cannot show slab fracture as in Li et al.; the flow reads more fluid
  than slab-like ([05-tuning.md](05-tuning.md)).
- **Speed against com1DFA.** The flows run about twice as fast as com1DFA's with the same
  Coulomb friction ([15-entrainment.md](15-entrainment.md)).
- **No CK-MPM** [6]: it would change only the P2G/G2P transfers.
- **Not validated against observed avalanches**, only against com1DFA. Educational use only,
  as the proposal states.

## Audit reports

Issue #3's paper-to-code audit of 2026-10-03, in [audit-report/](audit-report/). They are
historical records that describe the code as it was then. The fixes that followed are in
[07-constitutive-models.md](07-constitutive-models.md) §2h; their checks on the device are in
[06-verification.md](06-verification.md) §4j.

| File | What's in it |
|---|---|
| [report-implementation-audit-final-2026-10-03.md](audit-report/report-implementation-audit-final-2026-10-03.md) | **Final audit**: both independent audits merged, findings by severity with their source, comparison of the two, and the merged recommendations. |
| [report-implementation-audit.md](audit-report/report-implementation-audit.md) | First audit: paper-to-code comparison, deviations, verification boundaries, and recommended next steps. |
| [report-implementation-audit-opus.md](audit-report/report-implementation-audit-opus.md) | Second, independent audit, checked against the paper texts. |
