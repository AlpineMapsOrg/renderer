# AvaFrame comparison — phase 1: dataset preparation

`prepare_dataset.py` turns the AvaFrame test cases into a sandbox the MPM solver can be run
and compared on. That covers the DEM, the release thickness, the com1DFA reference results and the AIMEC
path. For real datasets it also writes PNG overlays for weBIGeo's Overlays panel. Those put
the com1DFA result on the map at its true location.

This is phase 1 of the AvaFrame comparison plan (Notion: "AvaFrame comparison"). Later phases:
2. a sandbox terrain node in the app;
3. peak rasters from the MPM solver;
4. display;
5. AIMEC;
6. model comparison.

## Setup

```bash
git clone --depth 1 --branch 2.1 --filter=blob:none --sparse https://github.com/OpenNHM/AvaFrame.git extern/avaframe
git -C extern/avaframe sparse-checkout set avaframe/data benchmarks
python3 -m venv mpm-mls-doc/avaframe/.venv
mpm-mls-doc/avaframe/.venv/bin/pip install -r mpm-mls-doc/avaframe/requirements.txt
```

`extern/` is git-ignored, so neither the clone nor the output is committed. The tag is pinned
(2.1, commit `9c3cefb`). Every `dataset.json` records the tag and commit it was built from.

## Usage

```bash
mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py avaKot
mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py --real
mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py --all
```

`--all` takes about 7 s and writes about 240 MB to `extern/avaframe_sandbox/`. The output is:

| File | Content |
|---|---|
| `index.json` | AvaFrame version; per dataset: location, release scenarios, references |
| `<name>/dataset.json` | Grid, altitude range, CRS and transformation used, release volumes, reference metadata |
| `<name>/dem.asc` | DEM, normalised: xllcorner form, float, NODATA −9999 |
| `<name>/release_<scenario>.asc` | Release thickness [m] per DEM cell (slope-normal, see below), 0 outside |
| `<name>/reference/<benchmark>/{pft,pfv,ppr}.asc` | com1DFA peak flow thickness [m], velocity [m/s] and pressure [kPa], on their own grid, plus `params.json` |
| `<name>/path.json` | AIMEC avalanche path and split point, in source coordinates and lat/lon |
| `<name>/preview_<scenario>.png` | Check image: hillshade, release (cyan), com1DFA footprint outline (green), AIMEC path (red) |
| `<name>/overlay/*.png` + `*_aabb.txt` | Real datasets only: com1DFA peak thickness and release area, reprojected to EPSG:3857 |

To view an overlay, load the `.png` in the Overlays panel → Texture overlay. The panel finds
the `_aabb.txt` next to it by name. Its format is four lines (x_min, y_min, x_max, y_max) in weBIGeo world
coordinates, the same as the app's exports.

## Datasets (AvaFrame 2.1)

**Real terrain** (MGI / Austria Lambert, EPSG:31287, 5 m DEM). These have com1DFA null-test
references and an AIMEC path:

| Dataset | Location (lat, lon) | Release scenario: volume | com1DFA footprint (pft > 0) |
|---|---|---|---|
| avaKot | 47.2595, 10.3988 | relKot: 21 681 m³, mean slope 44.6° | 11.7 ha |
| avaGar | 47.0471, 11.3397 | relGar: 61 828 m³; relGar2: 335 266 m³; relGar6: 110 211 m³ | 28.2 / 60.5 / 41.7 ha |
| avaHit | 47.1150, 11.6728 | relHit: 267 918 m³ | 120.8 ha |
| avaWog | 47.1287, 10.2821 | relWog: 165 292 m³ | 54.3 ha |
| avaMal | 47.0231, 10.2817 | relMal1to3: 438 846 m³ | 133.3 ha |

Also real, with 1 m DEMs: avaHof (no reference) and avaPfa (snow-glide test, 0.2 ha). They
are geolocated, but they are small slopes rather than avalanche paths.

**Synthetic** (no geolocation, no overlays): avaBowl, avaDamBreak, avaFPtest, avaFlatPlane,
avaHelix, avaHelixChannel, avaHockeyChannel, avaHockeySmall, avaInclinedPlane, avaParabola,
avaParabolaTimeDep, avaPyramid, avaPyramid45, avaSimilaritySol (no release shapefile), avaSlide,
avaTripleBowl.

**Skipped** (GeoTIFF DEM, not read yet): avaAlr, avaFlowPy, avaMalRegional, scarpExample.

## Conventions — read before comparing numbers

- **Release thickness is slope-normal.** AvaFrame's `thickness` attribute is measured
  perpendicular to the slope. The vertical depth is thickness / cos(slope), and the volume is
  Σ thickness · cell area / cos(slope). With that, avaKot comes to 21 681 m³ against com1DFA's
  logged 21 640 m³ (0.2 %). Taking the thickness as vertical undercounts by about 30 % on a
  45° slope. `dataset.json` gives both `volume_m3` (surface) and `volume_projected_m3`.
- **com1DFA results are on their own grid.** com1DFA remeshes to exactly 5 m, so `reference/`
  keeps its own header. Compare on that grid, or resample explicitly; don't assume it matches
  `dem.asc`. Previews and overlays resample with nearest neighbour.
- **References are the AvaFrame benchmarks** (`benchmarks/<name>NullTest*`). These are the
  standard com1DFA runs: samosAT friction (samosATSmall for avaKot, the smallest release), ρ = 200 kg/m³, run with AvaFrame v1.11. avaPfa's snow-glide test differs: it uses
  Voellmy and ρ = 300 kg/m³. Each `params.json` holds the benchmark's
  "Simulation Parameters" from its `_desDict.json`. A benchmark is matched to its release scenario through
  "Release Area Scenario". Only runs with `simType` "null" (no entrainment, no resistance)
  are used.
- **Geolocation.** The transformation is pyproj's most accurate available MGI → WGS 84 one,
  which is recorded in `dataset.json` (typically a 1.5 m Helmert). Synthetic datasets ship
  the same Austria Lambert `.prj` with local coordinates around (0, 0). Taken at face value,
  that puts them in the sea off Liguria, so a dataset is geolocated only if its DEM centre lies
  inside the CRS's area of use.
- **weBIGeo's world coordinates are Web Mercator**: 1 world unit = cos(lat) metres (see
  `04-data-layout.md`). The overlay rasters keep the 5 m source resolution on the ground, so
  their pixel size in world units is 5 / cos(lat).

## Phase 2: the sandbox in the app

In the Avalanche sidebar section, **AvaFrame sandbox** loads the graph preset
`mpm_avaframe_sandbox.json` (`AvaFrameSandboxNode` → `MpmSolverNode`). Pick the dataset and the
release scenario there. The run shows in the **AvaFrame sandbox** window: hillshade, release (cyan),
com1DFA footprint outline (green), the particle raster and the domain box (white). Hover gives
position, height and release depth. Material, friction and playback are the usual sidebar
controls. Native builds find the datasets in `<source>/extern/avaframe_sandbox`; the folder is
stored per user (`sandbox/root`).

What the node does with a dataset:
- **Release depth.** AvaFrame's slope-normal thickness becomes vertical depth, thickness / cos(slope),
  with the slope from central differences on the DEM. The solver seeds particles in proportion to
  the depth, each standing for release volume / particles (03-shaders, `mpm_seed`). The avaKot
  volume comes out at 21 681 m³, the same as Phase 1.
- **NODATA** is filled from the nearest valid cell (breadth-first), so the terrain continues as
  a plateau at the edge height. The plan said "walls". A wall tall enough to stop the flow would
  exceed the step the terrain-following band can hold ((layers − 4)·dx) and lose particles, so
  the plateau is the safer choice. Particles that reach it are past AvaFrame's DEM, and the
  2D view shows those cells darkened.
- **Domain.** A square around the release and the com1DFA footprint, with 15 % margin (at
  least 100 m) on each side; the whole DEM where there is no reference. Cell size = max(DEM
  cell, domain / 512): avaKot gets a 1760 m box at 5.0 m cells.
- **No map position yet.** The domain box is in sandbox metres. Phase 4 puts real datasets on the map.

First run (avaKot / relKot, Li 2021 V parameters, 65 536 particles, 2026-10-10, M5):
- All particles seeded.
- Faster than real time (85 s simulated in about 65 s).
- The centre of mass drops from 1921 m to 1094 m over 1372 m of mean path.
- The flow spreads across the valley floor well beyond the com1DFA footprint, and reaches the
  domain walls after about 55 s.

These numbers are only a smoke test. Quantitative comparison is Phase 5.

## Phase 3: comparable output

The solver tracks, per grid column and since the reset, the **peak flow depth** (the column's
mass as snow volume per area) and the **peak mean particle speed**. The latter is from a small
extra pass that runs only in the sandbox; 03-shaders explains why it is particle-based and a
separate pass. The run **stops like com1DFA's**: in the sandbox the panel pauses once the
kinetic energy is below 1 % of its peak (the benchmarks' desDict: "Stop criterion: < 1.00
percent of PKE"; checkbox "Stop at rest"). **Export** ("Export now", or "then export" at rest) writes AvaFrame's raster layout
next to the dataset:

```
extern/avaframe_sandbox/<dataset>/mpm/<yyyyMMdd-HHmmss>_<release>/
    pft.asc   peak flow thickness [m], slope-normal like com1DFA: vertical depth * cos(slope)
    pfv.asc   peak velocity [m/s], mean speed of the particles in the grid column
    ppr.asc   peak pressure [kPa] = rho * pfv^2 / 1000 (com1DFA's definition)
    params.json  dataset, release volume, solver settings, material preset, grid, stop reason,
                 simulated time, GPU time per substep, energy-line mu_eff, maxima, footprint areas
```

The rasters are on the DEM grid with the DEM's header (EPSG:31287 origin), so they overlay the
com1DFA references directly. Each value comes from the nearest grid column at the cell centre,
and is 0 outside the simulation domain.

First result (avaKot / relKot, Li 2021 V parameters, Coulomb mu 0.49, 65 536 particles,
4.99 m cells, M5, 2026-10-10). Reference is avaKotNullTest:

| | MPM | com1DFA (avaKotNullTest) |
|---|---|---|
| stop (KE < 1 % of peak) | 95.4 s | run time 117.6 s |
| max pft | 13.4 m | 5.9 m |
| max pfv | 104.5 m/s | 39.7 m/s |
| footprint | 71.1 ha (pft > 0), 17.9 ha (pft > 0.1 m) | 11.7 ha (pft > 0) |
| reference cells covered by MPM pft > 0.1 m | 97 % | |
| MPM cells (pft > 0.1 m) inside the reference | 64 % | |

Notes:
- The 71 ha at pft > 0 is a sub-centimetre layer of single particles. One particle on a 5 m cell
  is 1.3 cm deep.
- The speed is what Coulomb friction without a velocity-dependent drag allows on an 800 m drop:
  sqrt(2gh(1 − mu/tan theta)) is about 90 m/s. samosAT's turbulent term keeps com1DFA near 40 m/s.
- AIMEC (Phase 5) and the model variants (Phase 6) make this a proper comparison.

## Phase 4: display for the comparison

**2D view.** The sandbox window has a **Layer** picker:
- the live particle raster;
- the MPM peak thickness, velocity or pressure, read back from the GPU about once a second;
- com1DFA's peak thickness, velocity or pressure.

MPM and com1DFA layers share one colour scale (viridis over 0–5 m and 0–100 m/s, log over
1–1000 kPa; legend above the view). Cells under 0.1 m peak thickness are drawn faint, which
for the MPM is the sub-centimetre layer of single particles. Hovering shows MPM and com1DFA
side by side at the cell. That makes it an interactive version of the plots in the earlier
weBIGeo evaluation (`docs/Evaluation.md` on `release/lawinensymposium2025`). The layer and
whether the window is open are remembered.

**On the map (real datasets).** The graph has an `OverlayRenderNode` ("Map Overlay", stitch
trim off) fed by the sandbox node. It draws the release (cyan), the MPM peak thickness above
0.1 m (same colour scale) and the com1DFA footprint outline (green) on weBIGeo's own terrain.
Loading a real dataset flies the camera there; "Show on the map" does it again.
- **Placement.** The sandbox node fits an affine map from sandbox metres to Web Mercator to the
  DEM's four corners in `dataset.json` (pyproj, EPSG:31287 → WGS 84). It resamples into a
  texture aligned with the world axes.
- **Checked against pyproj's exact transformation** on a 21 × 21 grid over each DEM:
  largest error 0.26 m (avaKot), 0.24 (avaGar), 0.29 (avaHit), 0.31 (avaWog), 0.49 m (avaMal);
  mean 0.1–0.2 m. That is below the datum transformation's own accuracy (0.5–1.5 m) and a
  tenth of a DEM cell.
- **This shows place, not physics.** weBIGeo's terrain is not AvaFrame's DEM, so the overlay
  shows where the avalanche is, not how it would run on the map's terrain.

## Phase 5: rating with AIMEC

AvaFrame's own evaluation (AIMEC, Fischer 2013) compares an MPM run with the com1DFA benchmark
in the coordinates of the avalanche path. AvaFrame 2.1 needs Python < 3.14, so it gets its own
venv, built from the system Python 3.9:

```bash
/usr/bin/python3 -m venv mpm-mls-doc/avaframe/.venv-avaframe
mpm-mls-doc/avaframe/.venv-avaframe/bin/pip install avaframe==2.1
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/compare_aimec.py --latest avaKot
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/compare_aimec.py extern/avaframe_sandbox/avaKot/mpm/<run>
```

For each run, `compare_aimec.py` builds an AvaFrame avalanche directory in `<run>/aimec/<variant>/`:
- AvaFrame's inputs for the dataset;
- the benchmark rasters as module `com1DFA`;
- the MPM rasters as module `mpm`, under com1DFA-style names.

It runs AvaFrame's module comparison (`dfa2Aimec.dfaBench2Aimec`, `ana3AIMEC.mainAIMEC`) with
**the benchmark's own `*_AIMECCfg.ini`** (domain width, start of the runout area). AIMEC's plots
land in `Outputs/ana3AIMEC/com1DFA_mpm/`, and a summary in `<run>/aimec.json`. There are two
threshold variants:
- **ppr > 1 kPa**, AvaFrame's default runout criterion. It also counts the MPM's sub-centimetre
  particle layer wherever those particles are fast.
- **pft > 0.1 m**, the flow thickness, which ignores that layer.

What the numbers mean (AvaFrame's definitions, `aimecTools.py`):
- `sRunout`: path coordinate of the last cross section whose maximum exceeds the threshold.
  The **runout difference** is MPM minus com1DFA.
- `deltaSXY`: each result's own length along the path, first to last such section. It is **not**
  a difference, despite the name.
- `runoutAngle`: travel angle from the first to the last such section, along the thalweg.
- `TP`, `FN`, `FP`: areas in the **runout area only** (from `startOfRunoutAreaAngle` on), with
  com1DFA as the reference. Recall = TP/(TP+FN), precision = TP/(TP+FP).

### Batch mode

`WEBIGEO_SANDBOX_BATCH=<jobs.json>` makes the app run a list of sandbox runs one after the
other. Each run goes to rest (com1DFA's criterion) or to its time limit and is exported with its
label. The app then writes `<jobs.json>.results.json` and quits. A batch never changes the stored
user preferences.

```json
{ "runs": [
    { "label": "liV_voellmy", "dataset": "avaKot", "scenario": "relKot",
      "material_preset": "Vallee de la Sionne 2003 (Li 2021 V parameters)",
      "settings": { "basal_friction_model": 1, "terrain_friction": 0.155, "voellmy_xi": 4000 },
      "max_time": 400 } ],
  "quit": true }
```

`settings` takes any key of `MpmSolverNode::serialize_settings`. `max_time` defaults to 400 s,
com1DFA's tEnd. `params.json` records:
- the wall time since the reset and the GPU time (timestamp queries × substeps), for the
  computing-time comparison;
- the label and the full solver settings.

Checked (avaKot, two batch runs, 62 s in all): the Coulomb run reproduces the earlier manual
run exactly (deterministic solver). AIMEC (ppr > 1 kPa / pft > 0.1 m):

| run | runout difference | runout angle (com1DFA 31.0°) | runout area recall | precision |
|---|---|---|---|---|
| Li V, Coulomb mu 0.49 | +198 m / +130 m | 28.2° / 29.1° | 1.00 / 1.00 | 0.17 / 0.42 |
| Li V, Voellmy mu 0.155, xi 4000 | +291 m / +281 m | 27.2° / 27.3° | 1.00 / 0.70 | 0.11 / 0.13 |

## Phase 6: comparing models

| step | command | time |
|---|---|---|
| com4FlowPy, AvaFrame defaults | `.venv-avaframe/bin/python run_com4flowpy.py avaKot avaGar …` (`--reuse` converts an existing output) | 5–50 min per dataset |
| MPM variants and the weBIGeo trajectory model | `WEBIGEO_SANDBOX_BATCH=mpm-mls-doc/avaframe/batch_phase6.json <app>` | about 25 min for 35 runs |
| AIMEC on every labelled run, then the tables | `.venv-avaframe/bin/python summarise.py --jobs 4` | about 15 min |

The batch runs the trajectory model with `"model": "trajectories"`:
- preset `trajectories_avaframe_sandbox.json`: sandbox → normals → `ComputeAvalancheTrajectoriesNode`;
- on the DEM grid (resolution multiplier 1);
- with the earlier evaluation's parameters.

The MPM runs on the **whole DEM** (`domain_size_xy` clamped to its longer side, 512 cells). The
suggested domain around the com1DFA footprint cut off every run that went further. The first
batch showed it: different models stopped at the same distance.

`summarise.py` writes `results/phase6.csv` and `results/phase6.md`. Runouts that reach the end
of the AIMEC path are marked "≥" (lower bounds). Results and discussion:
[../11-avaframe-comparison.md](../11-avaframe-comparison.md).

## Robustness runs, screenshots and the detailed report

`batch_full.json` contains 65 MPM runs on the whole DEM:
- three materials, each with Coulomb and with Voellmy;
- Li 2021 cases I–IV with Coulomb;
- Li V with μ 0.40 / 0.47 / 0.55.

With `"screenshots": true` the batch pauses after each export. It shows the 2D sandbox view with
the MPM peak thickness and asks for a screenshot, then hides the view so the map overlay is visible
and asks again. `tools/run_batch_shots.sh` starts the app and answers each request with a capture
of the app's own window (macOS). The images land in the run folder as `sandbox.png` and `map.png`:

```bash
mpm-mls-doc/avaframe/tools/run_batch_shots.sh mpm-mls-doc/avaframe/batch_full.json /tmp/batch.log   # about 65 min
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/summarise.py --jobs 4
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/report.py   # extern/avaframe_sandbox/report/
```

For the map shot, each real dataset is framed by a camera looking steeply from the south at the
release and com1DFA footprint (`AvalanchePanel::frame_on_map`).

**Each batch run now sets its own time step: 0.8 × the CFL bound of its grid and material.**
Before, the preset and the domain setup only ever lowered dt, so a run inherited the time step of
the run before it. Results then depended on the order of the batch. All MPM results in
`results/` are from runs with the per-run time step.

`report.py` builds one HTML page with:
- the summary, the findings and the setup, including the material table;
- the robustness tables;
- per avalanche and run: the AIMEC numbers, a standard comparison image drawn from the
  rasters (the same for every model, including com4FlowPy and the trajectories), and the two app
  screenshots.

## Not done yet

- Phase 1's overlay PNGs, for the Overlays panel, are superseded by Phase 4's in-app overlay,
  but kept: they come straight from pyproj.
- Why Voellmy with com1DFA's parameters gives so little drag in the MPM (see 11, finding 3).
- GeoTIFF DEMs.
- Entrainment and resistance benchmarks. They are ignored on purpose: the MPM solver has
  neither.
