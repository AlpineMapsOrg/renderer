# Claude Session Log

Running log of Claude Code sessions on this repo: what was asked, what was decided, what
was actually verified, and what was left open.

**Convention.** Newest session at the top. One `##` section per session, dated. Within a
session, one `###` per distinct request. Record *decisions and their reasons* and
*verification evidence* — not a keystroke-by-keystroke transcript. Explicitly separate
"verified" from "assumed"; a log that overstates confidence is worse than no log.

This file is written by Claude and is a reconstruction from session context, not a verbatim
capture of the terminal.

---

## 2026-10-10

### 1. Solver in metres instead of Web Mercator units (branch `mpm-metric-units`)

Found while planning the AvaFrame comparison: `MpmSolverNode` took world units (Web Mercator,
1/cos(latitude) too long horizontally) as metres, while heights are metres. **Measured** before
changing anything, with a temporary probe comparing the slope the solver sees against the
Mercator-corrected release mask (30–45°): 25.3–27.3° on all three scenarios, i.e. tan 37°/1.47.
**Fix:** region size, domain position and release centre converted to metres with
cos(latitude of the region centre); domain box converted back for the map; panel readout from
the node's metric domain size. **Measured after:** 35.0–37.4°. Probe removed. Doc: 04 (coordinate
conventions), 06 §4k, 01 §5, 05. **Consequence:** all earlier real-terrain results ran on
flattened slopes; Dachstein now produces band-ceiling contacts on its cliffs. PR #6.

### 2. AvaFrame comparison, phase 1: dataset preparation (branch `avaframe-comparison`)

The plan (in Notion) compares the MPM solver with AvaFrame com1DFA on AvaFrame's own test
cases. Phase 1 makes those cases usable: `mpm-mls-doc/avaframe/prepare_dataset.py` reads a
pinned sparse clone of AvaFrame 2.1 (`extern/avaframe`, git-ignored). It writes, per dataset, to
`extern/avaframe_sandbox/`:
- the DEM;
- the release thickness rasters;
- the com1DFA benchmark peak rasters (pft/pfv/ppr);
- the AIMEC path;
- a preview PNG;
- for real terrain, EPSG:3857 PNG overlays with `_aabb.txt` sidecars for the Overlays panel.

It needs only numpy and pyproj: shapefile, dBASE, ASCII-grid and PNG I/O are hand-written.
The tool's README has the dataset table and the conventions.

**Decisions.**
- **Release thickness is slope-normal**, so the volume is Σ t·A / cos(slope). Treating it as
  vertical made avaKot 30 % light.
- **References stay on com1DFA's own 5 m grid**, because it remeshes. They are not
  resampled onto the DEM.
- **Geolocate only if the DEM centre lies inside the CRS's area of use.** The synthetic
  datasets ship the same Austria Lambert `.prj` with local coordinates, which would place
  them off Liguria.
- **GeoTIFF datasets are skipped** (avaAlr, avaFlowPy, avaMalRegional, scarpExample).

**Verified:**
- `--all` runs (23 datasets prepared, 4 skipped).
- avaKot release volume is 21 681 m³ against com1DFA's logged 21 640 m³.
- The avaKot preview matches AvaFrame's own plot of footprint and path.
- The overlay sidecar matches the parser in `nucleus/utils/geopng_decoder.cpp`.
- Real datasets land at 47.0–47.3° N, 10.3–11.7° E.

**Not verified:** overlay alignment on the map in the app.

**Not done:** phases 2–6 (sandbox terrain node, MPM peak rasters, display, AIMEC, model
comparison).

### 4. AvaFrame comparison, phase 2: the sandbox in the app

PR #6 (`mpm-metric-units`) was merged into this branch first, because the sandbox needs the
solver in metres. Then:
- **`AvaFrameSandboxNode`** loads a prepared dataset: DEM, release, and the com1DFA footprint
  resampled onto the DEM.
- **`MpmSolverNode`** gets an optional `sandbox` input. When it is connected:
  - the region is in metres;
  - the domain is centred on release plus reference footprint;
  - seeding uses rejection sampling by release depth per cell (`release_mode = 1`);
  - the volume per particle is the exact release volume divided by the active particles.
- The uniform grows from 176 to 192 B; the benchmark is updated to match.
- The release binding is now `unfilterable-float`, so it accepts both RGBA8 and R32Float.
- The **SandboxView** window and the panel's dataset picker, plus startup keys for running
  straight into a dataset.

**Decisions.**
- **NODATA filled as a plateau, not walls.** A wall would exceed the terrain-following band's
  step limit.
- **The suggested domain decides the size and the cell size:** dx = max(DEM cell, size/512).
- **No first-run notice for the sandbox preset.**

**Verified:**
- WGSL validator and the macOS build.
- avaKot/relKot with Li V parameters runs end to end:
  - all 65 536 particles seeded, release volume 21 681 m³;
  - 85 s simulated in about 65 s;
  - the centre of mass moves north as in AvaFrame's plot;
  - screenshot of the 2D view checked.
- Benchmark in the browser pane, this tree against HEAD: identical physics trace and GPU time
  (the disc path is unchanged).
- The map scenario runs without validation errors.

**Not verified:**
- The headless CDP benchmark script returned zeros on both trees. It is a harness problem,
  because the browser pane works.
- The web and iOS builds of the new files (no sandbox folder there by default).

### 5. AvaFrame comparison, phase 3: peak rasters, stop rule, export

- **Peaks per grid column** (`ColumnStats`, binding 11): depth from the column's mass, as for
  Voellmy, and speed from the mean speed of the particles in the column.
- **Stop rule** `at_rest()`: kinetic energy below 1 % of its peak, the benchmarks' "< 1.00
  percent of PKE". The panel pauses there.
- **Export** `write_sandbox_result`: pft (slope-normal), pfv and ppr (ρv²) on the DEM grid,
  with the DEM header, in `<dataset>/mpm/<time>_<release>/`, plus params.json.

**Two problems found and fixed on the way:**
1. **Grid velocity is the wrong speed.** Using the node momentum, or the updated grid velocity,
   gave up to 234 m/s at the flow's edge, where columns hold almost no mass and the MLS-MPM
   stress impulse dominates. The fastest particle was 101 m/s. Gating by peak depth did not
   help, because depth and speed peaks happen at different times. Now particle-based.
2. **Two atomics in P2G changed the physics trace.** They shifted it by 2 mm after 10 s,
   deterministically. Bisected in the benchmark: without the atomics the trace is
   bit-identical, and with them it differs even when they add a constant. That is the
   compiler's float arrangement, not data flow. They moved to a separate `mpm_column_stats`
   pass that runs only in the sandbox; the trace is bit-identical again and the GPU time
   unchanged.

**Verified:**
- avaKot: at rest after 95 s. Max pft 13.4 m (com1DFA 5.9 m), max pfv 104.5 m/s (39.7). The
  speed is right for Coulomb friction without drag.
- avaKot with pft > 0.1 m: 97 % of the reference cells covered, 64 % of the MPM cells inside
  the reference.
- avaGar: at rest after 116 s; 88 % covered, 79 % inside. The 35 m maximum sits at the gully
  constriction where com1DFA also peaks (12.3 m).
- Export headers equal the DEM's.
- Benchmark bit-identical to phase 2.

### 6. AvaFrame comparison, phase 4: display

- **2D view layers:** live particles; MPM peak pft, pfv and ppr; com1DFA pft, pfv and ppr.
  One colour scale for MPM and com1DFA, a legend, values under 0.1 m faint, and a hover table
  with MPM against com1DFA at the cell.
- **The sandbox node now loads all three reference rasters.**
- **Map overlay for real datasets.** An affine fit to the DEM corners, resampled into a
  world-aligned texture, through an `OverlayRenderNode`. That node gets a `trim_stitch_edge`
  switch, because its trim is for stitched tiles only. The texture is never null, since
  `TextureOverlay::render` asserts on it; synthetic datasets get 1x1 transparent.
- The camera flies to real datasets, wired to the controller like the search panel.
- The view's layer and visibility are stored per user.

**Verified:**
- Screenshot of the 2D peak layer with legend and hover table.
- Screenshot of the overlay on the map at the Lechtal: the fan sits at the slope foot.
- Affine placement against pyproj's exact transformation over each real DEM: ≤ 0.49 m,
  mean 0.1–0.2 m.

**Process note:**
- Screenshots now capture only the app window, matched by process ID. A full-screen capture had
  caught another window, and the first by-name match hit the user's own webgpu_app instance.
- Test runs now back up and restore the user's app preferences and imgui.ini. They had
  overwritten startup preset, location and pacing; those were restored by hand.

### 7. AvaFrame comparison, phase 5: AIMEC rating and batch mode

- AvaFrame 2.1 installed in `.venv-avaframe`. It needs Python < 3.14 and Homebrew has only
  3.14, so the venv uses the system Python 3.9.6; nothing installed system-wide.
- `compare_aimec.py` builds one AvaFrame avalanche directory per run: dataset inputs, the
  benchmark as module com1DFA, the MPM as module mpm. It runs AvaFrame's module comparison with
  the benchmark's own AIMEC configuration, in two threshold variants (ppr > 1 kPa,
  pft > 0.1 m), and writes `aimec.json`.
- Batch mode via `WEBIGEO_SANDBOX_BATCH`. Each run is exported with its label; the app writes a
  results file and quits without touching the preferences (checked by comparing the plist).
  `params.json` gains wall time and GPU time.

**Pitfall caught:** AIMEC's `deltaSXY` is each result's own length along the path, not the runout
difference. The first version reported 1767 m "difference". It is now sRunout(MPM) −
sRunout(com1DFA), and the area indicators are labelled as runout-area only.

**Verified:**
- AIMEC runs on avaKot. Its comparison plot shows MPM ⊇ reference with lateral spread.
- The batch reproduces the manual run exactly.
- Voellmy (com1DFA's mu 0.155, xi 4000) runs further than Coulomb on avaKot: +291 m against
  +198 m (ppr).

### 8. AvaFrame comparison, phase 6: models compared

- **com4FlowPy** through AvaFrame (`run_com4flowpy.py`).
- **weBIGeo trajectory model** in the sandbox: a new preset, a release mask from the sandbox node,
  `export_trajectories`, and the batch `"model": "trajectories"`.
- **35-run batch** (`batch_phase6.json`). `summarise.py` runs AIMEC and writes the tables, and
  `11-avaframe-comparison.md` holds the write-up.

**Two measurement problems found and fixed before reporting:**
1. **Runs cut off by the MPM box.** Different models ended at exactly the same runout per
   dataset. The suggested domain (com1DFA footprint + 15 %) stopped them. Rerun on the whole DEM;
   runs now record their wall contacts, which are 0–0.1 %.
2. **Runs reaching the end of the AIMEC path.** A remaining identical value turned out to be the
   end of AvaFrame's path. compare_aimec records the path length and marks such runouts "≥"
   (lower bound).

**Result** (pft > 0.1 m, five avalanches):
- **Cam Clay with Li V parameters and Coulomb is closest.** Mean |Δrunout| 135 m, |Δangle| 1.5°.
- **Every Coulomb variant except Stomakhin is about 2–2.5× too fast.**
- **Voellmy with com1DFA's μ 0.155 / ξ 4000 runs too far for every material.** Open: the flow
  depth the drag uses.
- **Stomakhin's film snow stops early on the long avalanches.**
- **The trajectory model is good where com1DFA's angle is near its α of 25°** and overshoots
  elsewhere.
- **com4FlowPy, on all five after the PR (11–146 min each):** mean 186 m and 2.5°. It sits
  between the trajectory model and the best MPM variant.

### 9. AvaFrame comparison: robustness runs, screenshots, detailed report (2026-10-11)

- **65 MPM runs** (`batch_full.json`): materials × friction, Li I–IV, and a μ sweep for Li V.
- **Screenshots of every run.** The batch has screenshot phases; `tools/run_batch_shots.sh`
  captures by process id. Each real dataset is framed on the map by an explicit camera
  (`frame_on_map`), because the search fly-to hid north-facing gullies.
- **Report.** `report.py` builds an HTML report with 205 images, published as a private claude.ai
  artifact.

**A reproducibility bug was found and fixed:** dt was only ever lowered, so a batch run inherited
the previous run's dt. A test run of Li V on avaKot gave 112 m/s against 100 m/s inside the batch.
Each batch run now sets 0.8 × CFL for its own grid, and all MPM runs were redone.

**Result:**
- **Li I cold dense with Coulomb μ 0.47 fits best:** 103 m mean runout difference, 1.4°.
- **Li V:** 140 m at μ 0.49, 111 m at μ 0.47.
- **The cohesive Li II–IV stop short.**
- **One μ cannot fit all five avalanches.** The steeper ones fit at 0.55, the flattest at 0.40.
  That points to the friction law.
- **First draft corrected:** it called that pattern a size effect, but avaHit (268 000 m³) fits
  at 0.55. The doc now relates it to com1DFA's travel angle instead.

### 13. Rendering research and plan, new worktree branch (2026-10-18)

**Request:** start a worktree branch for the rendering ("Partikelsystemrendering, potentiell
aggregierte Partikel"), research MPM and particle rendering, report with options, link in
Notion.

**Done:**
- Worktree `.claude/worktrees/mpm-rendering` on branch `worktree-mpm-rendering`, from main
  (`836dbcec`).
- Report [Avalanche Particle Rendering](https://claude.ai/artifact/PWiuxTHUn9LvobGEQpksXj),
  linked on the Notion rendering page and the overview.
- Plan in `mpm-mls-doc/12-rendering-plan.md`.

**Decision proposed:**
- G-buffer geometry, so the avalanche is lit like the terrain.
- Sphere impostors, then ellipsoids from the deformation gradient F (PhysGaussian's covariance
  idea).
- A draped flow surface aggregated per grid column (as Vicari et al. 2025 do for GIS).
- Transparent powder later.
- Not chosen: marching cubes (grid too coarse) and screen-space triangulation.

**Sources:** some were read through search summaries only (the WebGPU-Ocean write-up blocked
direct access); the report says which.

**Implemented the same day: steps 1–2 (plumbing, sphere impostors).** The user asked to start
and allowed edits to existing files. Details are in `mpm-mls-doc/12-rendering-plan.md`
("Status").

- **Solver:** `MpmParticleView` (a "particle view" output).
- **App:** a `ParticleRenderNode`.
- **Engine:** `AvalancheRenderer` and `render_avalanche.wgsl`, drawn in the G-buffer pass.
- **Panel:** the "3D particles" controls.
- **Validator:** `validate_wgsl.py` extended to engine shaders.

**Worktree build:** `extern` is symlinked to the main checkout's (prebuilt Dawn, sources that
include `extern/...`). Do not commit the symlink.

**Verified:**
- tint validates all shaders.
- The macOS debug build compiles.
- A sandbox batch run on avaKot (25 s) shows the lit particles at the front on the map, with
  no WebGPU errors.
- Prefs and `imgui.ini` were restored, and the test run folder was deleted.

**Not verified:** the web build, a close-up.

**Map mode is the target** (user, same day). The node was removed from the sandbox preset.

**Map-mode verification:** Hintertux at 132 s, lit particle deposit and release. It used a
temporary test build without the first-run notice; the preset was restored and rebuilt.

**Step 3, strain-rate ellipsoids, done the same day.**
- **The plan's premise was wrong:** the solver stores only the elastic F, about 1 % for snow, so
  F-ellipsoids would look like spheres. The user chose the strain-rate variant (from C).
- **Added:** a "Look at the flow" camera button.
- **Verified:** Hintertux at 77 s shows stretched moving particles, through a temporary
  auto-framing test build that was reverted.

**Step 4, flow surface, done the same day.**
- **Design:** bin → depth (solver terrain) → draped grid mesh with 3×3 smoothing and a 5 cm
  contour.
- **First attempt:** used the lowest particle as "ground"; it cut into slopes and looked blocky.
  Replaced by the depth pass.
- **Verified:** Hintertux at 74 s, via a temporary test build that was reverted.
- **Open:** isolated small patches.

**Fixes after step 4 (user):**
- **Patches:** a 3×3 neighbourhood count of at least 8 is needed for a column to join the
  surface.
- **Overlay:** the 2D density overlay is hidden while the 3D particles are on, through a new
  `Overlay::enabled`. The ping-pong chain uses only the enabled overlays.
- **Verified:** with the temporary test build, reverted afterwards.

**Approach that failed:** posting a click to the app's process (`CGEvent.postToPid`) does not
reach SDL in the background. I did not move the user's real mouse.

**Detail particles (2026-10-23, branch `mpm-rendering`, uncommitted).** The user reported a
flickering, aliased flow surface and particles that look out of place because there are too
few.
- **Literature:** Stomakhin 2013 never shows particles; it rasterises them with the B-spline
  kernels and path-traces a volume (0.3–7.2 M particles). Hu 2018 describes no rendering.
- **The user chose both fixes:** render-only child particles first, then a mass-splat flow
  surface.
- **Children:** K per particle (1 / 8 / 27, default 8). Offsets are hash-stable, the radius is
  scaled by K^(-1/3), and the children are scattered within 2 parent radii. They fade back
  into the parent below about 3 px. Details are in `12-rendering-plan.md`, step 5.
- **Two layouts were rejected after an A/B screenshot.**
  - The volume-equivalent ball (0.62 spacing) made the clusters smaller than single particles.
  - Packing the children into the parent's own sphere made them read as clods.
- **Verified** with a temporary test build: paused at 40 s, camera at 150 m, the same frame
  rendered with 8 and with 1.
- **Test trap:** the app redraws only on change. Switching a setting from code while paused
  needs a camera nudge to show up; the UI combo triggers the redraw itself.
- **Test runs** set `startup.autoplay` temporarily, inside the prefs backup and restore.
- **Temporary test patches** were reverted from the pre-patch copies and the build was
  re-run.

**Mass-splat flow surface (same day, uncommitted).** It replaces the per-column binning. Details
are in `12-rendering-plan.md`, step 6.
- **Splat:** a B-spline splat at the solver's dx onto a 3× finer render grid (shaders renamed
  bin → splat, depth → field).
- **Depth:** volume / area decides where the blanket is drawn.
- **Height:** 2 × the mean particle height + radius, tapered at the edge.
- **First attempt:** the volume depth alone. The blanket sat at release density under a looser
  moving flow, and the particles speckled through it everywhere. Hence the height term.
- **Verified:** the paused A/B at 3× vs 1× (smooth vs faceted outline) and a burst during
  playback (no popping).
- **Open:** GPU time not measured; the web build is not tested.

**Gaps over bumps: diagnosis, then track and snow cover (same day, uncommitted).** Details are
in `12-rendering-plan.md`, step 7.
- **Diagnosis:** the same paused frame, with and without the airborne cut-off and with a
  one-particle threshold. The holes stayed, so they are real: no particles within 12 m. They are
  not a rendering threshold.
- **B, track:** a persistent peak depth per render node, cleared on solver reset (new
  `MpmParticleView::reset_count`). It draws a 0.3 m duller layer where the flow has been.
- **A, snow cover:** `ParticleRenderNode` owns a pre-shading (lit) `ScreenSpaceSnowOverlay`.
  The avalanche writes a marker into the G-buffer overlay slot, which the snow overlay skips.
- **Test trap 1:** setting the same camera again does not redraw. Nudge the distance (800,
  800.5, 801 m) when switching settings while paused.
- **Test trap 2:** log the stage switches (`qWarning`) and match them to the capture
  timestamps. The time to reach a given simulated time varies by several seconds between
  runs.
- **Validator:** now also checks `overlays/screen_space_snow`; temporary files for nested
  names are flattened.
- **Requested by the user:** future rendering options (screen-space fluid rendering and others)
  documented in Notion.

**Powder particles (same day, uncommitted).** The user's idea: particles rendered like gas, large,
dense in the middle and sparse outside, as a guide over the surface. Mid-turn the user added: a
noise texture shifted per particle. Details are in `12-rendering-plan.md`, step 8.
- **Shape:** a third particle shape, `Shape::Powder`.
- **Pass:** transparent, after compose, on the track-line pattern. No sort is needed for
  same-coloured premultiplied "over" blending.
- **Soft intersection** via the G-buffer depth.
- **Noise:** procedural per-particle value noise instead of a texture asset (same effect, nothing
  to load).
- **Verified** on one paused frame, A/B against the ellipsoids. GPU cost not measured.

**Powder default 8× spacing (user), committed with the powder particles. Snow cover thinning
with altitude (same day, uncommitted).** The user asked for less snow lower down, then
mid-turn: "possibly also just more sparse with a noise texture".
- **Snow line:** the avalanche's snow cover fits it to the solver's terrain scan: full above
  55 % of the relief, none at 15 % below the lowest point.
- **Patches:** a new `patches` setting on the screen-space snow overlay turns the falloff into a
  covered share drawn as world-space noise patches, instead of translucency.
- **Verified:** an overview and a close-up of Hintertux.

**Powder is now the default particle shape** (user). `Settings::shape` = Powder; the map preset
sets no shape, so it applies there.

**Docs brought up to date, towards main (same day).** The other session's docs fixes (PR #10,
branch `claude/dreamy-sagan-1uw3da`) were merged into `mpm-rendering`.
- `README` (doc table, pipeline, status), `02-files-and-api` (rendering files, modified engine
  files, the `particle view` output, panel 3D controls), `03-shaders` (rendering shaders),
  `06-verification` §5b (rendering checks; §5 now points at the AvaFrame comparison),
  `12-rendering-plan` status.
- Merging PR #10 into main from here was refused by the permission check (merge without
  review). Left for the user.

### 12. Entrainment research: a snow cover made of particles (2026-10-12)

**Request:** assess the idea of seeding particles along the whole path, held by friction and
picked up by the flow. First from the implemented papers, then for feasibility in our sim.
Write the report in Notion.

**Done:** Notion page "Entrainment with a particle snow cover — research and feasibility
(12.10.2026)", under Avalanche Sim, and a short pointer in `07-constitutive-models.md` §7.

**Sources read in full or in large parts** (PDFs via a scratch pypdf venv):
- Gaume et al., ISSW 2024 O2.5;
- Kyburz et al. 2024 (Salezer);
- Li et al. 2024 (Communications Earth & Environment);
- Li et al. 2020 (The Cryosphere), via the web page;
- the com1DFA entrainment theory page.

Li et al. 2022 is known only from its abstract and the ISSW summary. Cicoira et al. 2022 is
cited, not read.

**Conclusions:**
- **The idea is the state of the art,** but it needs a cover at least 1–2 cells thick. Our
  cells are 5–12.5 m, as tall as they are wide.
- **Why it fails literally here:**
  - Coulomb μ 0.47 holds a cover only below about 25°.
  - The cover and the flow share one node layer and the friction band.
- **Feasible: dormant cover particles** (mass < 0), woken in G2P, which behaves like
  com1DFA's plowing mode.
- **Cost:** with the AvaFrame ENT areas at 0.3 m, the cover adds 10–82 % particles
  (+6–54 k). A uniform cover over the whole DEM needs 0.3–3.1 M particles.

**Nothing implemented.**

### 11. prepare_dataset.py ported to C++, behind a download button (2026-10-11)

**Request:** port the preparation script to C++, trigger it from a button, and show a popup
saying the data is external, may disappear, and takes a while.

**What was built:**
- **`apps/webgpu_app/avalanche/AvaFrameData.{h,cpp}`.** It covers the five real datasets.
  - It downloads 62 files (about 40 MB) from raw.githubusercontent.com, pinned to the 2.1
    commit, with a hardcoded manifest.
  - It parses the ESRI grids, shapefiles and dbf tables, rasterises the release polygons,
    projects EPSG:31287 to WGS 84, and writes the same files as the script.
  - It merges its entries into `index.json` and deletes nothing.
  - Not ported: the preview and overlay PNGs, which nothing reads, and the synthetic datasets.
- **Panel.** "Download AvaFrame data..." in the sandbox section opens a modal: external data
  that AvaFrame may move or remove, about 40 MB, takes a while, where it is saved, data sources.
  While it runs, the panel shows a progress bar, the status and a cancel button. When it
  finishes, the panel lists the datasets again and reloads the selected one.
- **Default sandbox root without a source tree** (web, iOS): the app data folder; on the web
  that is browser memory.

**Decisions:**
- **Number parsing and formatting** go through Qt (`QByteArrayView::toDouble`,
  `QByteArray::number`), because Qt sets the C locale from the environment and
  `strtod`/`printf` would then use a German decimal comma. Floating `std::from_chars` is
  unavailable for this macOS target.
- **Exact ties are rounded to even,** like Python's `%.3f`. Without that, 56–192 DEM cells per
  dataset came out 1 mm different.
- **The projection is written out** instead of using PROJ: inverse LCC on Bessel 1841, then
  "MGI to WGS 84 (8)", a 7-parameter Helmert in coordinate-frame convention. That is the
  transformation pyproj picked for the script.
- **Threads.** Preparation runs on `QThreadPool` natively. The web build runs it queued on the
  main thread. Results carry a run counter, so a cancelled run's result is dropped.

**Verified:**
- **Output against the script.** A scratch harness built `AvaFrameData.cpp` alone and ran the
  real download into a scratch folder, in 1.2 s. Compared with `prepare_dataset.py`'s output:
  every `.asc` is byte-identical, and every JSON file has the same content.
- **Projection against pyproj:** at most 0.05 mm apart across Austria.
- **Builds.** The macOS debug and wasm release builds compile, with no warnings from the new
  code.
- **Web fetch.** In the served wasm page (cross-origin isolated, COEP `require-corp`), both
  fetch and XHR to the pinned raw URLs return 200.

**Verified by the user:** the panel flow at run time works (popup, download, reload). I could
not drive it myself: the browser pane was hidden, so the page ran at one frame per second.

Mistake on the way: the harness took a quoted coordinate pair as an output folder and twice
wrote stray folders into the repository root. Both were removed; the harness now requires
`--out`.

### 10. Where the AvaFrame test data comes from, before redistributing it in the web build (2026-10-11)

The user decided to bundle the five real avalanches with the web build rather than load them
from AvaFrame's repository at run time. They asked first where the data originally comes from.

- **Terrain (DEM).** It is Land Tirol's "Digitales Geländemodell Tirol" (5 m) from Open Data Austria.
  The dataset's licence on data.gv.at is **CC BY 4.0** on every distribution. Verified through the
  hub API, record `0454f5f3-1d8c-464e-847d-541901eb021a`. The required attribution is
  "Datenquelle: Land Tirol - data.tirol.gv.at".
- **Release areas and thicknesses.** These came from the Austrian Torrent and Avalanche Service
  (WLV, die-wildbach.at), through FZGL Fachbereich Lawinen, in November 2020. The dbf comments
  ("Gutachterlich auf Basis Ereignis …") show they are expert assessments from hazard zoning.
  AvaFrame's dataSources page says the data was "kindly provided" and names **no licence**.
  No open-data listing for it was found.
- **AvaFrame.** The repository is EUPL-1.2 as a whole, with one `LICENSE.txt` and no separate
  data licence. `MANIFEST.in` prunes `avaframe/data`, so the PyPI package does not contain the
  data; only the Git repository does.
- **com1DFA reference rasters.** We generate these ourselves with AvaFrame code from the
  inputs above, so they are derived from both sources.

**Open:** whether the WLV release data may be redistributed outside AvaFrame. It is
distributed publicly in AvaFrame's EUPL repository, but nothing states the WLV's terms.

**Decision:** do not redistribute. The sandbox stays native-only, and the reason is written in the
meeting notes of 13.10. and in `mpm-mls-doc/11-avaframe-comparison.md` §4.

### 3. iOS build (PR #5, merged) — log entry added after the fact

webgpu_app runs on an iPhone 17 Pro Max. The build uses:
- Qt 6.11.1 iOS through aqt;
- Dawn built from source for Metal, with a BC-format patch in `UtilsMetal.mm`;
- static SDL2;
- the Xcode generator.

The fixes needed on the device were:
- a UIScene manifest;
- `SDL_Metal_CreateView` for rotation;
- a smaller cloud atlas against the jetsam memory kill;
- a repaint after resize, so the map is not black after a screenshot;
- safe-area insets;
- a UI scale of 0.75.

Documented in `docs/webgpu_app_ios.md`.

**Open:**
- 3× resolution;
- pinch zoom;
- whether the top menu is reachable with the safe-area fix (not confirmed on the device);
- replacing the temporary `TEMP-IOS-DIAG` log.

## 2026-10-03

### 1. Issue #3 audit, re-checked with Klár 2016 and Wolper 2019

Manuel added both PDFs. Klár confirmed the Drucker–Prager code (§7.1 Eq. 27–28; Klár's flow
is itself non-associative) and showed the cited section numbers (§5.3/§5.4) are wrong.
Wolper showed the fixed-p Cam Clay return is NACC for Cases 1–2 but **omits §6.2.3**, the
shear hardening — so shear never softened. Final and Opus audit reports updated, pushed to
main (`f63e91e8`).

### 2. PR addressing the final report (branch `audit-fixes`)

**Done:** H1 (Wolper §6.2.3 in Case 3), M1 (plastic ratio per substep via
`PlasticReturn.yielded`), M2 (energy-line path = mass-averaged particle path, summed in G2P,
cumulative since reset), M3 (Voellmy local flow depth; real particle volume from a Monte
Carlo estimate of the seeded area in `mpm_seed`), M4 (attribution), M5 (box-contact
counters + sidebar warning), H2 (preset names/notes), L3, L5 (`max_stable_dt()` on P-wave
speed, enforced every run), D4, guard/deviation table in 01-theory. G2P diagnostics reduced
in workgroup memory. Bench: `SimState` 72 B, new `ccc_slab` material.

**Decisions.** §6.2.3 rather than the associative return: closed form, and it makes the code
the model it cites; associative stays a follow-up. Box counter uses stencil contact, not
clamps: a 640 m test box showed 0 clamps while the flow piled against the wall (the grid
wall nodes stop it first). ξ not rescaled with Δx: Li gives no rule.

**Verified:** all offline scripts; tint; native build; app runs on M5 for Li V, Li III,
Stomakhin, DP, Voellmy, partial-cover seeding and a small box (06 §4j). Headless-Chrome A/B
vs main: no measurable cost; Li V trace unchanged, Li III mean speed 19.4 → 18.1 m/s.
**Not verified:** web (wasm) build locally — CI builds it; behaviour on non-Apple GPUs.
**Not done:** associative Cam Clay return, Klár φ hardening, convergence study at two Δx,
GPU golden readbacks vs the Python ports, the DOI in issue #3's text.

## 2026-09-13

### 1. Proposal v3.0 gap analysis — constitutive & friction models

Read the Notion proposal (now **v3.0**, edited today) and the new *Snow/Avalanche Sim Paper
Summaries* page, then checked each named model against the shader source.

**v3.0 changes vs v1.0:** constitutive model becomes a configurable choice (Stomakhin [5] or
successors Gaume CCC [11], Li et al. [12,13]) with per-regime presets; basal friction is
explicitly a separate simple Coulomb model using terrain normals; entrainable material is a
new goal; comparison against non-real-time models is a new final stage.

**Verified implemented:** Stomakhin 2013 in full (fixed-corotated elasticity, SV box clamp,
exponential hardening, paper defaults). Coulomb basal friction at grid + particle level using
terrain normals — exactly what v3.0 asks for, though μ defaults to 0.4 where Li 2021 uses
0.47 on real terrain.

**Not implemented:** CCC, Li regime parameters/presets, Drucker–Prager, Voellmy, any model
*switch* at all (Stomakhin is called unconditionally), entrainment, energy-line validation.

Wrote `mpm-mls-doc/07-constitutive-models.md`: the gap table, the design for exchangeable
models, and an ordered next-steps list. Key design decision: follow the codebase precedent
in `ComputeAvalancheTrajectoriesNode` — C++ enum → `u32` uniform → runtime `switch` in WGSL
→ ImGui combo — rather than compile-time `///if` variants, because the preprocessor's
defines are global and a uniform branch has no divergence cost. Two *separate* enums
(constitutive vs basal friction) so internal and basal friction cannot be conflated, which
both the proposal and the paper summaries warn about. Per-particle state stays one scalar
for all three candidate models, so `Particle` does not change (`jp` → `plastic_state`).

Next-steps order deliberately puts the no-physics refactor first, then Drucker–Prager
(closed-form, validates Hencky + the dispatcher) *before* CCC (implicit ellipse return
mapping, the hard part, and the documented fallback target if CCC is too slow).

### 2. "yes start with step 1" — model dispatcher refactor, no new physics

Made the constitutive model and basal friction exchangeable without changing behaviour.

**Shaders.** Moved the Stomakhin model out of `mpm_common.wgsl` into
`mpm_material_stomakhin.wgsl` (`stomakhin_initial_state/stress/plasticity`). Added
`mpm_material.wgsl` — three `switch settings.constitutive_model` dispatchers — and
`mpm_friction.wgsl` — Coulomb behind `switch settings.basal_friction_model`. `PlasticReturn`
(the return-mapping result type) lives in `mpm_common` as shared interface. `p2g`/`g2p`/`seed`
call the dispatchers; `grid_update`/`g2p` include the friction module. Renamed `Particle.jp` →
`plastic_state` everywhere. `mpm_common` no longer injects material/collision code into
kernels that never used it (`prepare`, `splat`, `rasterize`, `clear_grid`).

One interface addition beyond the plan: `material_initial_state()`. The initial plastic state
is model-dependent (Jp = 1 for Stomakhin, εᵥᵖ = 0 for CCC/DP), so `mpm_seed` cannot hard-code
`1.0`.

**C++.** `enum ConstitutiveModel { STOMAKHIN_2013 }`, `enum BasalFrictionModel { COULOMB }`;
settings fields; uniform +16 B (144 → 160, `static_assert` updated); serialised as ints;
`Combo` in both the node renderer and the sidebar, with Stomakhin's parameters shown under an
`if` on the active model. Three shaders added to the CMake resource list.

**Verification — the point of step 1 is that nothing changed:**
- `tint`: 8/8 pass with the modular includes (the preprocessor is pragma-once, confirmed in
  `ShaderPreprocessor.cpp:199`, so modules can `///use mpm_common` for themselves).
- **Resolved-shader diff against git HEAD** (`scripts/check_refactor_preserving.py`): resolves
  includes on both sides, extracts every top-level definition, normalises the intentional
  renames, folds the dispatcher wrappers away. Result: every retained function body identical
  modulo renames; the only definitions that vanished from a kernel are ones it never referenced;
  `MpmSettings` +4 fields exactly. Two rounds of false positives were bugs in the checker
  (renaming a call site also renamed the dispatcher's *definition* and clobbered the real one in
  the dict; dict keys weren't renamed alongside bodies), not in the refactor.
- Runtime: booted with the MPM graph, all 8 pipelines created on Metal, no Dawn errors.
  Temporary boot/log changes reverted.

Docs updated: 02 (files, enums), 03 (modules section), 04 (160 B, `plastic_state`), 05 (the
"adding a constitutive model" recipe), 06 (§4b resolved-shader diff), 07 (status reconciled,
step 1 ticked), refs.

### 3. "yes continue with step 2" — μ → 0.47, Voellmy basal friction

**Changes.** `terrain_friction` default 0.4 → **0.47** (Li et al. 2021 Table 1, real terrain);
preset updated. `BasalFrictionModel::VOELLMY = 1` with `voellmy_friction()` in
`mpm_friction.wgsl`: Coulomb plus the turbulent term, `τ = μσₙ + ρg|v|²/ξ` → deceleration
`g|v|²/(ξ·h)`. Used `h = slab_thickness` as the reference depth because that is the
convention both com1DFA and `ComputeAvalancheTrajectoriesNode` use (the latter hard-codes
`h = 1 m`), so ξ stays in literature units — default 4000, with a note that the conventional
pairing is a lower μ ≈ 0.155. `voellmy_xi` took the spare `_pad_a` slot in the uniform, so no
layout growth. Combos in both panels gained "Voellmy" and a ξ slider shown only when active.

**A subtlety worth recording.** `resolve_terrain_collision()` is called at both the grid
and the particle level. Coulomb is a contact *impulse* depending on `vn`, which is ≈ 0 by the
particle-level call, so double application is benign. Voellmy's drag depends on `|v_t|²`, not
`vn`, so the same double call would apply it **twice per substep**. Added an
`apply_basal_drag` flag: `true` in `mpm_grid_update`, `false` in `mpm_g2p`. Coulomb ignores
it. This distinction — impulse terms are level-agnostic, velocity-dependent drag is grid-only
— is now written into the friction module header and `05-tuning.md` for the next model.

**Verification.**
- New `scripts/test_friction.py`: verbatim port, point mass on an inclined plane stepped the
  way `mpm_grid_update` does it. Voellmy reaches the analytic terminal velocity
  `√(ξh(sinθ − μcosθ))` to **0.085 %** (explicit-Euler lag); Coulomb's slope acceleration
  matches `g(sinθ − μcosθ)` to 3 dp (regression for the untouched path); both stick below
  atan(μ); `apply_basal_drag = false` reduces *exactly* to Coulomb; a 500 m/s / dt = 1 s
  reversal guard holds.
- `check_refactor_preserving.py` extended (flag stripped at call sites, Voellmy defs
  allowed) and re-run against pre-step-1 HEAD: still behaviour-preserving for the Coulomb
  path with steps 1 + 2 combined.
- `tint` 8/8; boot with MPM graph, 8 pipelines on Metal, no Dawn errors; temporaries reverted.

Docs: 01 (Voellmy formula + terminal velocity), 02, 03 (flagged call sites, seed init), 04,
05 (params table, the impulse-vs-drag rule), 06 (§4c friction bench), 07 (step 2 ticked, §2b
rewritten), README, refs (Tonnel 2023, Li 2021 entries).

### 4. "continue with step 3" — Drucker–Prager (Klár 2016)

First real second *constitutive* model, deliberately before CCC: its return mapping is a
closed-form projection, and it brings in the Hencky elasticity CCC will reuse.

**Implementation.** `mpm_material_drucker_prager.wgsl`, `ConstitutiveModel::DRUCKER_PRAGER = 1`.
Hencky strain `ε = log Σ`, Kirchhoff stress `τ = 2με + λ tr(ε)` in the principal frame,
`P Fᵀ = U diag(τ) Uᵀ`. Yield cone `‖dev τ‖ + α tr τ ≤ 0`, `α = √(2/3)·2sinφ/(3−sinφ)`
precomputed CPU-side from a `dp_friction_angle` setting (default 30°). Return mapping is
Klár §5.3 Cases I/II/III. `plastic_state` accumulates δγ; hardening of φ left out.

**Two design changes against the plan in `07`, both recorded there.** (a) E/ν are *shared*
across models instead of duplicated as `hencky_*` — two stiffness knobs that silently
disagree is worse than one, the CFL readout keys off the shared E, and presets are the
right place for per-model defaults. So DP needed exactly one field (`dp_alpha`, last pad
slot, uniform still 160 B). (b) `DRUCKER_PRAGER = 1` not 2, so the combo stays contiguous
until CCC exists.

**Bug caught by the test before it shipped.** The first draft guarded Case II with
`‖ε̂‖ = 0 OR tr ε > 0`, which sends *pure hydrostatic compression* to the cone apex and
drops all elastic strain. Hydrostatic compression has zero deviatoric strain but sits on
the cone's axis, inside the surface — it must stay elastic. With `tr ≤ 0` the second term
of δγ is ≤ 0, so `δγ > 0` already implies `‖ε̂‖ > 0`; the division guard was unnecessary
and the condition is `tr > 0` alone. Test case 1 in `test_material_dp.py` pins it.

**Verification.**
- `test_material_dp.py`: 12 checks. The substantive one — every Case III projection lands
  *on* the cone, `|y| < 1.5e-10` over 500 random gradients; never outside it. Idempotence
  check needed loosening from "case label is I" to "state doesn't move": an on-cone state
  has `y ≈ +1e-12` and the re-projection fires Case III with δγ ≈ 1e-15. Test strictness,
  not physics.
- `test_mpm.py` parametrised by `MPM_MODEL`, reusing the DP port. Stomakhin regression
  unchanged (jp 0.7456). DP on the same drop: mean z 3.65 and still falling vs 4.00 stable;
  max|v| 2.7 m/s still spreading vs 0.06 at rest; plastic strain growing 0.25 → 0.92 vs
  saturated. Cohesive piles and stops, cohesionless keeps spreading — the expected physical
  difference, with no tuning.
- `tint` 8/8; 8 pipelines on Metal; temporaries reverted.

Docs: 01 (§4b Hencky + cone), 02, 03, 04 (no pad slots left → next field is 176 B), 05
(shared-E rule), 06 (§4d), 07 (§2d incl. the bug and the design changes; step 3 ticked),
README, refs (Klár 2016).

### 5. "commit step 3 then continue with step 4" — Cohesive Cam Clay (Gaume 2018)

Step 3 committed as `be83247d`. Then the model the proposal's paper survey recommends.

**Implementation.** `mpm_material_ccc.wgsl`, `ConstitutiveModel::COHESIVE_CAM_CLAY = 2`.
Hencky elasticity (shared with DP). Gaume's ellipse `(1+2β)q² + M²(p+βp₀)(p−p₀) ≤ 0` with
`p = −K tr ε`, `q = √(3/2)·2μ‖dev ε‖`; hardening `p₀ = K sinh(ξ max(−α,0))`, α the
plastic volumetric strain. `plastic_state` = α, initialised to `−asinh(p₀ⁱⁿⁱ/K)/ξ` so
`p₀(α₀) = p₀ⁱⁿⁱ` — the reason `material_initial_state()` had to exist. Four new uniform
fields; **uniform 160 → 176 B**, no pad slots left. Defaults are Li 2021 Case V (the
real-avalanche back-calculation): M 0.7, β 0.2, ξ 0.002, p₀ 3 kPa.

**The return-mapping decision, made explicit.** Gaume describes an associative flow rule,
which needs a per-particle Newton solve on the ellipse. Implemented instead is the
three-case projection of Wolper et al. 2019 (NACC) — the same group's own implementation
of this yield surface and hardening law: cap → `(p₀,0)` + harden; tensile tip → `(−βp₀,0)`
+ soften; shear → `q` onto the ellipse at fixed p. Explicit (old p₀ for the projection).
Both papers cited; the docs say which is which; swapping in an associative return only
touches `ccc_plasticity()`. Checked on paper that Case 3's `q_new/q` division is safe: with
p in range the ellipse term is ≤ 0, so `y > 0 ⟹ q > 0`.

**Verification.**
- `test_material_ccc.py`, 12 checks — the yield surface and hardening law are closed-form,
  so every projection is checked directly. α₀ round-trips exactly; Case 3 lands on the
  ellipse (`|y| ≈ 7e-8` on a 1e7 scale) *at the same p*; Case 1 returns to `(p₀,0)` and
  hardens (3000 → 3012); Case 2 returns to `(−βp₀,0)` and softens; 500 random gradients
  never leave the surface; repeated tension softens monotonically to **exactly p₀ = 0**
  (fracture); repeated compression hardens until admissible then stops; β = 0 has no
  tensile strength; all five Li 2021 Table 1 cases produce finite α₀ that round-trip at
  E = 3 MPa. Two first-run failures were test strains too small to reach the surface
  (0.2 % expansion = −467 Pa vs 600 Pa strength) — inputs, not model.
- `test_mpm.py` with `MPM_MODEL=ccc`. **The parameter → behaviour link works**: weak Case V
  flattens to a pancake (mean z 3.02) because a 14 kPa impact is far above the 3 kPa cap
  and Cam-Clay *loses* shear strength above p₀ — the opposite of DP's cone; strong Case III
  (42 kPa) piles at 4.13 alongside Stomakhin's 4.00. Same code, four parameters. Both come
  to rest; DP doesn't.
- `tint` 8/8; 8 pipelines on Metal; temporaries reverted.

Docs: 01 (§4c, incl. the "softer than DP above p₀" point), 02, 03, 04 (176 B), 05
(params row — p₀ is the knob), 06 (§4e with the four-way drop comparison), 07 (§2e,
step 4 ticked, v3.0 table: all three named models done), README, refs (Wolper 2019).

**Not done in step 4, flagged in 07:** benchmark it. One SVD + scalars per particle, same
order as Stomakhin, so the expectation is fine — but that is an expectation.

### 6. "commit step 4 then continue with step 5" — regime presets + diagnostics readback

Step 4 committed as `90991efa`.

**Presets.** `AvalanchePanel::MaterialPreset`, seven entries: Stomakhin 2013; Li 2021
Cases I–IV and V; a Drucker–Prager cold-dense fallback (φ ≈ 13.3° from M = 0.5). Presets
*write the ordinary settings* — model, E, ν, ρ, μ, model params — nothing more, so there is
one source of truth and a preset can be edited afterwards. Applying one forces a reseed and
pulls dt under the new CFL bound (3 MPa is ~6× the wave speed of 0.14 MPa; the first run
would otherwise explode). Hand edits drop the combo to "(custom)".

**Bug found while doing it.** Switching the material combo did not reseed. `plastic_state`
is model-specific: Stomakhin's `Jp = 1` read by Cam-Clay is `α = 1` → fully softened,
`p₀ = 0`, a material that carries no stress. Latent since step 3. Both panels now force
`reset_on_next_run` on any model change.

**Readback.** `SimState` +`plastic_particles` (24 B); counted once per run in `mpm_splat`
as particles whose state left `material_initial_state()`. The state buffer is now
allocated once in the constructor — fixed-size, and an async readback in flight across an
`ensure_resources()` reallocation would be a use-after-free. Per-run counters zeroed by a
2-slot `write()` before non-reset runs so the terrain scan and seed count survive.
`read_back_state()` fires from the work-done callback into `last_state()`; sidebar shows
"N particles, X % plastic, max V m/s, terrain A–B m", plus a warning when 0 seeded.

One edit went wrong and was caught on re-read: inserting the `else { reset_run_counters() }`
split the block so the location log moved into the non-reset branch. Fixed before build.

**Verification.** `tint` 8/8, build clean. Then the real one — the readback is a path no
offline check covers — by temporarily auto-running the graph on load: 131072 active,
968 plastic (0.7 %), max 1.57 m/s (≈ g·sinθ·t for 0.24 s), terrain 1303–2060 m. Every
number plausible; first real-terrain diagnostic the solver has produced. Temporaries
reverted, tree clean.

Docs: 02, 03, 04 (SimState 24 B, allocation rule), 05 ("start from a preset", the
model-switch rule, two new failure-mode rows), 06 (§4f, bug #6), 07 (§2f, step 5 ticked;
v3.0 table now shows the whole material layer done), README.

### 7. "step 6" — energy-line validation (com1DFA §5.2)

Step 5 committed as `8779515d` first.

**What it measures.** Energy height `h_E = z_com + v̄²/2g` drops by exactly μ per
*horizontal* metre under Coulomb friction, whatever the slope, because the friction work
`μ m g cosθ ds` has `cosθ ds` = horizontal increment. So the slope of `h_E` vs horizontal
path is `−μ_eff`, and `μ_eff − μ` is internal dissipation. A force-balance check of the
whole loop that needs no real avalanche.

**Offline** (`test_energy_line.py`, `test_mpm.py` terrain generalised to a slope with the
flat regression unchanged at 150.00 / 3.00 / 0.7456). Slab on a 30° plane, two runs:
μ = 0 → `μ_eff = 0.0026` (energy conserved); μ = 0.3 → `μ_eff = 0.305`; **difference 0.3024
vs 0.3 expected** — under 1 %. The strongest single validation so far.

The first version *failed* and the failure was mine: terrain rises with +x so downhill is
−x, and the slab was seeded 2–7 m from the wall at x = 2. It slid into the wall and
reported `μ_eff = 0.91` on a frictionless plane and a *negative* basal contribution. Seeded
at the top of the slope instead; both runs then reach the same 87 % of ideal free-slide
distance, so the shortfall is startup geometry.

**On device.** `mpm_splat` sums position and `|v|²`; `read_back_state()` appends an
`EnergySample`; `energy_line_friction()` fits over the sliding regime (past 10 % of total
path — the seeded slab compacts before sliding). Sidebar shows `μ_eff`, set μ, difference,
sparkline.

**Bug caught by looking at the raw samples, not by a test.** First GPU version summed
`value/N × scale` to keep a mean inside an `i32`. Per particle that is
`floor(v²/131072 × 10⁴)` → 0 below 3.6 m/s. KE read **0 for the first three runs and ~30 %
low after** (sample 8: 9.79 vs 14.78 corrected). Replaced with real 64-bit sums: lo/hi `u32`
pairs, carry detected as `atomicAdd(lo, x) > 0xFFFFFFFF − x`. `SimState` 24 → 56 B.
On device after the fix: KE resolved from run 1; `μ_eff` 0.65 → 0.61 converging, ~0.59
over the last window vs μ = 0.47 — ~0.12 internal dissipation on real terrain, where the
smooth plane gave 0.003. Temporaries reverted; `NodeGraphPanel.cpp` back to committed.

Docs: 02 (API), 03 (the lo/hi carry trick), 04 (SimState 56 B, why 64-bit), 05 (reading
`μ_eff`), 06 (§4g, bug #7), 07 (§2g, step 6 ticked), README.

### 8. "there is still an issue that the simulation doesn't cool down" — fixed-point energy pump

Screenshot: Sliding slab (CCC), 127 s, 100 % plastic, max speed 17 526 m/s, `μ_eff` 0.31
and falling. The energy-line readout from step 6 was the instrument that made this
visible as *energy creation* rather than "it's fast".

**Facts established on device** (a temporary harness in `AvalanchePanel::draw()` applying
a preset, auto-playing and logging `t / vmax / plastic / μ_eff / path / z / h_E` per run):
model-independent (Stomakhin, DP, CCC all blow up); particle-count-independent; the energy
injected per *step* is independent of dt (dt = 0.005 blew up sooner); no particle ever
flies or sinks — it is purely tangential; dropping the APIC `C` from P2G removes it, halving
`C` delays it 10 → 55 s, RPIC (rotation-only `C`) and a `|C·dpos| ≤ |v|` cap delay it
without removing it. So `C` looked like the culprit and PIC would have "fixed" it — at the
cost of a solver that creeps at 4 m/s.

**Ruled out** (each tested, none changed anything): particle-level speed clamps, a
strain-rate limit on the F update, a contact band and a two-sided terrain BC, smoothed
terrain normals, CFL.

**What found it.** All offline plane tests were at `dx = 1`. A vectorised numpy copy of
the loop at *device* scale (`dx = 12.5`, 1.5 m slab = 0.12 cells, dt = 0.01, 4 k
particles, 35° plane, μ = 0.47) was still exact against the rigid block: 37.00 vs 37.0 m/s
at 20 s. Adding one line — emulate the shader's `i32(x · 1e4)` truncation per P2G
contribution — reproduced the blow-up: 161 m/s mean, node velocities > 1000 m/s, same
exponential shape as the device. Mechanism: truncation loses up to one unit per
contribution; for a mass contribution `w = 1e-3` that is ~10 %, for its momentum `w·v` at
10 m/s ~1 %, so `momentum/mass` is biased *high* at every low-weight node, per step,
independent of dt. A slab 0.12 cells thick gives its third z layer exactly such weights
for every particle, and APIC's `C` carries the excess back into the particles. (First
attempt at this offline reproduction had its own bug — `svd` returns `Vh`, I transposed it
— which produced a rotation matrix for F and a fake "elastic instability"; caught by
printing F.)

**Fix** (`mpm_common.wgsl`, `mpm_p2g.wgsl`, `mpm_clear_grid.wgsl`, `mpm_grid_update.wgsl`,
`GRID_NODE_STRIDE_U32` 4 → 5): round instead of truncate; mass in its own 64-bit `u32`
lo/hi pair at 2⁻²⁰ (same carry trick as `SimState`), momentum unchanged at 1e-4 in an
`i32`, now with the range limit stated (particles-per-node × speed < 2·10⁵). Emulated
offline: shader scheme −0.1 % vs block; rounding alone still drifted +2 %, truncation with
a fine mass was 8 % *slow* (momentum truncation is dissipative), hence both changes.

**On device after the fix**, same harness: Stomakhin — vmax 21–26 m/s for all 144 s,
`μ_eff` 0.58 → 0.49 (basal 0.47), stops after 380 m; Sliding slab CCC — vmax ≤ 25 m/s,
plastic fraction 25 → 55 % (was 100 %), `μ_eff` 0.60 → 0.49, stops after 340 m. Residual:
~90 of 131 k particles keep moving at > 10 m/s after the mass has stopped — lone particles
on a coarse grid see little basal friction; noted in 05 as cosmetic.

New: `scripts/test_sheet_fixed_point.py` (regression test, emulates both schemes). All
TEMP harness/instrumentation reverted; `SimState` back to 14 u32. Offline suite re-run:
`test_mpm.py` ×3, `test_energy_line.py`, `test_sheet_fixed_point.py` all OK.
Docs: 03 (P2G/grid pseudo-code), 04 (GridNode 20 B, "round, don't truncate"), 05 (two
new failure-mode rows), 06 (§4h, bug #8 with the detours), 07 (§2g addendum, step 6).

### 9. "work instead on getting the grid correct" — domain size analysis, then E + B

Step 7 (entrainment) was started and abandoned at the user's request; partial edits
reverted. Task: the simulation is boxed into a fixed area and clamps at the walls;
research fixes, list options, estimate the non-MPM code each touches (less = easier
upstream approval).

**Analysis** (`mpm-mls-doc/08-domain-size-options.md`). Two boxes: the terrain *region*
(`GeoRegionNode.extent` 2500 m, upstream limit 8192 px stitched ≈ 100 km at z15) and the
MPM *domain* (1600 m, 128³, dx 12.5). The domain was small because the grid was a dense
`res³` box while the snow lives in 2–3 of 128 layers — 98 % air, cleared and updated every
substep. Faces are frictionless walls (`grid_update` zeroes outflow, `g2p` clamps). Two
facts make fixes cheap: the grid holds no state between substeps, and `rerun()` chains
down the graph so the overlay re-reads the domain aabb every run. Options: A bigger dense
grid (settings only, still `res³`), B terrain-following band grid (K layers per column,
~150 MPM lines, 0 upstream), C sparse/blocked (rejected: hash table + activation in WGSL
for reach nobody needs), D moving window (0 upstream, ~80 MPM lines, stateless grid makes
it free), E bigger region (settings). All options: **0 lines outside files we own**.

**Measured** with a temporary harness on the M5 (reverted): per run of 24 substeps,
`ms ≈ 9 + 25·(nodes/10⁶) + 0.38·(particles/10³)`. Preset 116 ms; 512² × 16 band ≈ 157 ms
(real time = 240 ms); particles cap real time at ~550 k.

**Implemented E + B.** `mpm_common`: z grid coordinate is now absolute `altitude / dx`;
`column_floor[]` (binding 8) written by `mpm_prepare` as `floor(terrain/dx) − 2`;
`grid_slot(node)` maps a stencil node through its own column's floor and returns −1
outside the band; P2G/G2P skip those; `grid_update` iterates `(x, y, layer)` and uses
`column_node_world()`; `g2p` clamps particles below the band ceiling
`(floor + layers − 2.5)·dx`. `grid_resolution_z` → `grid_layers` (default 16),
`MAX_GRID_RESOLUTION` → `_XY` 512 and `MAX_GRID_LAYERS` 64; both panels, JSON key,
scenario table (region 8000 m, domain 4000 m, 320 nodes → dx 12.5), preset JSON (raster
1024). `domain_base_altitude()` gone; `min/max_altitude` readout only.

**Verified.** Offline: the sheet test with the band emulated (8 layers) matches the
dense grid exactly. Device, Breite Ries 144 s, band 320² × 16 over 4 km vs dense 128³
over 1.6 km: path 378.3 vs 380.7 m, z_com 1757.6 vs 1756.5, `μ_eff` 0.4904 vs 0.4903,
100 vs 116 ms per run. So the flow really does stop at 1757 m on its own (slope under
tan⁻¹ 0.47 ≈ 25°) — it was not the wall.

Docs: 02, 03 (prepare, grid update, stencil), 04 (band section, buffer sizes), 05
(settings table, failure modes, cost), 06 (§4i), 08 (marked implemented), README.
Committed by the user as `f02ad59a` "Extend terrain".

## 2026-09-28

### 1. "fix this" — validation errors from the timestamp resolve

Console full of `The destination buffer [mpm timestamp resolve] offset (16) is not a
multiple of 256` followed by `[Invalid CommandBuffer ...] is invalid due to a previous
error`. Not cosmetic: the rejected command buffer is the one carrying that chunk's P2G /
grid / G2P dispatches, so **every second chunk was dropped** — half speed, no GPU timing,
and the adaptive pacing running blind.

Cause: `ResolveQuerySet`'s destination offset must be a multiple of 256
(`QUERY_RESOLVE_BUFFER_ALIGNMENT`); the two timestamp slots were packed at their natural
16 B stride, so slot 0 was fine and slot 1 was not. Fix: `TIMESTAMP_SLOT_STRIDE_BYTES =
256`, resolve buffer sized `TIMESTAMP_SLOTS × 256 B`, offset `slot × 256`. Verified on
device: 0 validation errors over a 60 s run, 614 timestamp samples arriving,
`gpu_ms_per_substep` 0.55 → 1.7 as the flow spreads. Docs: 06 (bug 9), 09 (§9.2).

## 2026-09-14

### 1. "the simulation runs in the same thread as the normal rendering - can we separate this?"

Diagnosis first, with a temporary fps harness (reverted): 13–25 fps while playing. Not a
thread problem — the CPU side of a run is microseconds of command encoding and the
work-done callback. WebGPU has a single queue; a run submitted as one ~100 ms command
buffer parks the next frame's render behind it. A CPU thread changes nothing; a second
device would give a second queue but cannot share the overlay texture or the terrain.

Fix: `MpmSolverNode::submit_chunk()` — the run is submitted in chunks of
`substeps_per_submit` substeps (first chunk clears the raster and does prepare/seed on a
reset, last chunk splats and rasterises), re-armed from the queue's work-done callback.
One chunk in flight left the queue idle every other frame (the callback lands two frames
after submission), so two are kept in flight with a counter deciding when the run is
complete. Measured (M5, preset, vsync 60): one submit 25 fps / 2.0× real time; chunks of
2 → **60 fps / 1.65×**; 4 → 53 fps / 1.8×. Default 2, setting in both panels ("Substeps
per frame"), JSON key. Docs: 02, 05 (setting, "frame rate vs simulation speed").
Committed as `9fd10f0d`.

### 2. "Can i get a config setting that automatically loads the graph and applies it"

No config mechanism existed in the app (no settings file, no CLI parsing in our reach).
Added it to the panel we own, persisted with `QSettings("weBIGeo", "avalanche")`:
`AvalanchePanel::ready()` (runs after `NodeGraphPanel::ready()` loaded the default
graph) loads the MLS-MPM preset, applies the stored material preset and scenario
(`apply_scenario` runs the graph → terrain fetch → seed) and optionally starts playing
once `has_valid_inputs()`. UI: "Load this simulation at launch" + "and play"; the
scenario and material combos save their choice. When the active graph has no MPM node
the panel now offers a "Load the MLS-MPM simulation" button instead of hiding.
Upstream footprint: `NodeGraphPanel::load_preset()` moved to public (one line).
Verified on device: with the plist set, the log shows the 4 km run seeded and autoplay
starting at launch. Plist reset to off afterwards. Docs: 02, 05.

"But what about the webGPU build for chrome. I want that to be in the release
temporary": under `__EMSCRIPTEN__` there is no usable settings store (Qt's WASM
QSettings loads asynchronously), so the web build loads the simulation by default and a
URL parameter overrides it — `?avalanche=0` skips, `?avalanche=play` also plays — read
once with `EM_ASM_INT`. Verified in Chrome (browser pane, wasm release build behind a
COOP/COEP static server): default and `?avalanche=play` seed the 4 km run and play,
`?avalanche=0` shows the default graph with the panel's "Load the MLS-MPM simulation"
button. The graph's first-run notice text updated (it still pointed at the node editor).

### 3. Meeting notes and a sketch of the band grid

Notion page "Meeting Minutes 15.9.2026" written (papers, com1DFA rationale, what was
implemented, verification, open points, references) with the disclaimer that all of it
is Claude's work pending the user's own verification. Concept sketch of the
terrain-following grid — dense box vs band, cross-section with terrain, snow layers,
per-column floor and a particle's stencil — added as `mpm-mls-doc/terrain-following-grid.svg`
and explained in 04-data-layout.md.

## 2026-09-06

### 1. "Make a folder mpm-mls-doc and document ... for my final report"

Created `mpm-mls-doc/` — implementation notes in an informal, technical register (own-notes
style, not a paper), sized for reuse in the final report.

- `README.md` — index, 60-second orientation, honest status/gaps
- `01-theory.md` — FLIP -> MPM -> MLS-MPM -> snow model; only the maths actually implemented,
  with pointers into the papers; a table of every deliberate deviation from the literature
- `02-files-and-api.md` — files added/changed, classes, methods, the null-socket gotcha
- `03-shaders.md` — the eight kernels one at a time, dispatch order, why one compute pass
- `04-data-layout.md` — struct layouts, fixed-point atomics, the mass-normalisation
  argument, coordinate conventions, uniform alignment rules
- `05-tuning.md` — what each knob does, failure-mode table, the resolution problem, how to
  extend (incl. where CK-MPM and the 3D renderer would go)
- `06-verification.md` — what was tested vs what wasn't, plus the bug post-mortems
- `refs.md` — bibliography, numbering matching the Notion proposal
- `scripts/` — `validate_wgsl.py`, `test_svd.py`, `test_mpm.py`

Note on "quotes": interpreted as *citations*, not verbatim quotations — each section points
at the specific paper/section/equation it follows rather than reproducing text. Better for a
report anyway, since the wording has to be the author's own.

The verification scripts had been lost when the session scratchpad was cleared overnight, so
they were rewritten into `mpm-mls-doc/scripts/` with repo-relative paths and **re-run** to
confirm the documented numbers rather than quoting them from memory: 8/8 kernels pass
`tint`; SVD exact over 700+ matrices; MPM loop conserves mass at 150.00, rests at min z 3.00,
mean jp 0.7456.


## 2026-08-29

Model: Claude Opus 5 (repo overview answered by Sonnet 5 before the model switch).

### 1. "Can you give me an overview of the repo"

Explored the tree and [README.md](README.md). Summary of what this is:

AlpineMaps.org / weBIGeo mono-repo — C++ renderer for large-scale, tile-based alpine
geodata, with two frontends over a shared core:

- `app/` — Qt Quick/QML + OpenGL, the original alpinemaps.org client
- `apps/webgpu_app/` — weBIGeo: WebGPU renderer, ImGui UI, GPU compute node graph
- `nucleus/` — shared core (tiles, camera, avalanche report data, picker, vector tiles)
- `gl_engine/` — OpenGL engine for `app`
- `webgpu/` — `base/` (RAII wrappers, WGSL preprocessor, resource registry),
  `engine/` (renderer), `compute/` (node graph + WGSL kernels)
- `plain_renderer/`, `unittests/`, `extern/`, `docs/`

Note the README's mermaid diagram is slightly stale: it shows `webgpu_engine` and
`webgpu_compute` as separate top-level dirs; on disk they are merged under `webgpu/`.

Conventions: CamelCase classes, snake_case methods/vars, `m_` private prefix, namespaces
mirror folders, 4-space indent, `.clang-format` provided.

### 2. "Check my notion page … Is a good way to do this in apps/webgpu_app/compute/nodes"

Read the Notion proposal *Real-Time Avalanche Simulation and Visualisation in weBIGeo*
(MLS-MPM snow solver + a rendering technique, VisComp WS26).

**Answer: the architecture fits well, but `apps/webgpu_app/compute/nodes/` is the wrong
half of it.** That directory holds only thin ImGui/imnodes presentation wrappers
(`*NodeRenderer`), registered by `dynamic_cast` in `NodeRendererFactory::create()`. No GPU
state or compute logic lives there.

The solver belongs in `webgpu/compute/nodes/` (namespace `webgpu_compute::nodes`), where
`Node` subclasses own GPU buffers/pipelines and implement `run_impl()`. Two close
templates: `ComputeAvalancheTrajectoriesNode` (settings/uniform struct pattern) and
`IterativeSimulationNode` (already does a bounded multi-dispatch GPU loop decoupled from
the render loop — exactly the shape an MPM substep loop needs).

Also flagged: the proposal's "Particle Renderer" is not a compute-node concern.
`OverlayRenderNode` is a 2D overlay bridge and cannot express 3D particles; that needs a
new engine-side Renderer.

### 3. "Implement my proposal"

Found [AVALANCHE_SIM_PLAN.md](AVALANCHE_SIM_PLAN.md) already in the repo — a detailed
phased execution plan derived from the proposal. Followed it (it names the node
`MpmSolverNode`).

**Added**

| File | Purpose |
|---|---|
| `webgpu/compute/nodes/MpmSolverNode.{h,cpp}` | Solver node; owns particles/grid/state on GPU |
| `webgpu/compute/shaders/mpm_common.wgsl` | Shared bindings, terrain sampling, signed 3×3 SVD, Stomakhin model |
| `webgpu/compute/shaders/mpm_{prepare,seed,clear_grid,p2g,grid_update,g2p,splat,rasterize}.wgsl` | The eight kernels |
| `apps/webgpu_app/compute/nodes/MpmSolverNodeRenderer.{h,cpp}` | Settings panel + Play/Step/Reset |
| `apps/webgpu_app/resources/graphs/mpm_avalanche_simulation.json` | Ready-to-run example graph |

**Modified**: `webgpu/compute/CMakeLists.txt`, `webgpu/compute/NodeRegistry.cpp`,
`apps/webgpu_app/CMakeLists.txt`, `apps/webgpu_app/compute/nodes/NodeRendererFactory.cpp`,
`apps/webgpu_app/resources.qrc`, `apps/webgpu_app/compute/NodeGraphPanel.cpp` (preset list
entry "Avalanche simulation (MLS-MPM)").

**Key design decisions** (these resolve the open questions in plan §3)

1. **Particle mass normalised to 1**, volume set to `1/density`. Scaling mass and volume by
   the same factor leaves the MPM equations invariant, so this is a pure unit change — but
   it bounds the fixed-point grid accumulators independently of real snow mass, which is
   what makes a compile-time `FIXED_SCALE = 1e4` safe. Without it, avalanche-scale masses
   (~10^6 kg/node) would overflow an `i32`.
2. **One bind group layout for all eight kernels.** Every kernel includes `mpm_common.wgsl`
   and therefore declares an identical binding set, so a whole run — all substeps, splat,
   rasterize — fits in a *single compute pass*. Dispatches within one pass are ordered and
   see each other's storage writes, which is exactly the MPM dependency chain. Grid reset
   is a compute kernel rather than `clearBuffer` specifically so the pass never has to end.
3. **Vertical grid origin derived on-GPU.** A `prepare` kernel scans terrain in the domain
   footprint and `atomicMin`s the altitude into a state buffer; all later kernels read it.
   Avoids an async readback and avoids making the user guess a base altitude.
4. **Collision at both levels** — grid-node Coulomb friction plus a particle position
   clamp, since the node-level condition alone lets snow creep through between nodes.
5. Height sampled with **manual bilinear** — the stitched DEM is R32Float and unfilterable,
   so `textureSample` is unavailable.

**Verification performed**

- `tint` (from vendored Dawn) validates all 8 kernels.
- SVD ported verbatim to Python and checked against numpy over 700+ matrices: exact
  reconstruction, orthonormal U/V with det +1, correct on inverted (det F < 0) and
  near-degenerate inputs.
- Full substep loop ported offline: mass conserved exactly; free fall reproduces `g·t` to
  three decimals; snow lands, settles to rest (v 5.98 → 0.06 m/s), rests exactly on the
  floor; mean `jp` 1.0 → 0.746, i.e. plastic compaction on impact rather than an elastic
  bounce — the Stomakhin behaviour the proposal wanted.
- Booted the app with the MPM graph as startup preset and confirmed via a temporary
  `qInfo` that all 8 shaders compiled and all 8 pipelines were created on Metal with no
  Dawn validation errors. Both temporary changes reverted afterwards.

Incidental finding: `qDebug()` output is filtered in this app's logger; use `qInfo()` for
diagnostics that need to appear in the console.

**Explicitly NOT done**

- **Never run against real terrain.** All verification above covers the math and the
  plumbing, not the terrain coupling. Plan Phase 3's flat-synthetic-field check and Phase 4's
  real-DEM switch still need a human at the keyboard.
- **Phase 5 proper (3D particle rendering) not implemented.** Output is a top-down density
  raster through the existing overlay path (no engine changes). A real `AvalancheRenderer`
  in `webgpu/engine/` remains the right next step; the node already exposes a raw
  `particle buffer` socket so it can bind without CPU readback.
- Parameter tuning at avalanche scale — defaults are Stomakhin's paper values, authored for
  metre-scale snow, not a 1 km domain.

### 4. "export this conversation in a markdown file claude_log.md and keep that updated"

Created this file and recorded the standing instruction in Claude's project memory so
future sessions append to it.

### 5. "How can i test it now" / "How do i know in which region the avalanche is"

Two usability gaps surfaced while writing test instructions, both fixed:

- **No feedback when nothing seeds.** If the domain does not overlap a release area, every
  particle stays inactive and the overlay is simply blank — with no error. Added a
  **"Seed anywhere (ignore release areas)"** debug toggle that fills the whole domain, so a
  first run can confirm the solver works and show where the domain sits. Implemented by
  repurposing a spare `_pad0` float in the uniform, so the struct layout is unchanged.
- **No way to tell where the simulation is.** The region is not configured in the MPM node
  at all — it comes from the **GPX Input** node (`:/gpx/breite_ries.gpx`) via Select Tiles.
  Added a lat/lon readout of the domain centre to the settings panel plus a `qInfo` line on
  every reset (both via `nucleus::srs::world_to_lat_long`).

Location of the default test scenario, computed from the GPX file: **Breite Ries gully,
Schneeberg, Lower Austria**. Track lat 47.77442–47.77892, lon 15.80953–15.82461, elevation
1364–1931 m. Region = 2×2 zoom-15 tiles = 2446 × 2446 m. Default domain (centre 0.5/0.5,
1024 m) is centred at **47.77625, 15.82031**; the track centre sits at normalised
x = 0.352, y = 0.528, i.e. inside the default domain near its western edge — so the default
already covers the gully.

Also corrected misleading UI text: the settings panel only renders for the *selected* node,
and that is what drives stepping, so "keep this panel open" became "keep this node
selected".

### 6. SIGSEGV on toggling "Seed anywhere" — fixed

Crash reported immediately on ticking the new checkbox. Stack:

```
MpmSolverNodeRenderer::render_settings_content()
  -> Node::rerun() -> Node::run() -> MpmSolverNode::run_impl()
    -> update_gpu_settings() -> TextureWithSampler::texture()   <- null deref
```

**Root cause (my bug).** `run_impl()` validated inputs with `is_socket_connected()` only,
then dereferenced the socket data. But *connected is not the same as ready*:
`HeightDecodeNode`'s output socket returns `m_output_texture.get()`, which is **null until
that node has actually run**. `rerun()` re-runs only this node using the last buffered
context, so any UI control that calls it before a full graph run dereferences null.

This was latent in every transport control (Play/Step/Reset) — the checkbox just happened
to be the first one pressed before `Shift+R`.

**Fix**, in two layers:
1. `MpmSolverNode::has_valid_inputs()` checks connectivity *and* non-null payloads;
   `run_impl()` calls `fail_run()` with an actionable message instead of dereferencing.
   This makes the node safe regardless of who calls `rerun()`.
2. The renderer computes `ready` once per frame, wraps the transport buttons in
   `BeginDisabled`, clears `m_playing`, shows "Run the full graph once (Shift+R)", and gates
   the `rerun()` call — otherwise auto-play would spam error modals.

**Lesson for future node work in this repo:** an output socket returning `unique_ptr::get()`
is null before its node runs. Any code path that can trigger a single node out of graph
order must null-check payloads, not just check `is_socket_connected()`.

### 7. Visible, but only animating with the graph editor open — sidebar panel added

Two reports after the first successful run (9.36 s simulated, domain centre correct):

**a) Nothing visible on screen.** Not a solver bug — a display one. The domain was 1024 m
across a 1024-texel raster (1 m/texel) and `mpm_splat` wrote each particle into *exactly
one* texel. 65536 particles over 1,048,576 texels covered ~6% at count 1, i.e. isolated
1-metre pixels at ~37% alpha seen from kilometres away. Rendering was correct; what it was
told to render was invisible. Particles represent parcels of snow, not points.

Fixes: `mpm_splat` now draws a disc of `splat_radius` metres (default 6 m, loop capped at 8
texels); `mpm_rasterize` normalises against a CPU-computed `density_reference` (the coverage
a uniform spread would give) instead of a magic `/6.0`, so the display rescales itself when
particle count or resolution change; default raster 1024 -> 512. Uniform struct grew to
144 B. New "Splat radius" slider plus an "Output texel: X m" readout.

**b) Animation only advanced while the graph editor was open.** Root cause: stepping was
driven from `MpmSolverNodeRenderer::render_settings_content()`, which only runs while the
node graph editor is open *and* that node is selected.

Fix: new `apps/webgpu_app/avalanche/AvalanchePanel.{h,cpp}`, a sidebar panel that owns the
transport. Key detail of the `ImGuiPanel` interface: `draw_panel()` renders inside the
sidebar (so it stops when the section is collapsed), while **`draw()` runs every frame
regardless of panel visibility**. Stepping therefore lives in `draw()`, and the animation
keeps running with the sidebar section collapsed and the editor closed.

The panel resolves the solver by scanning `NodeGraphPanel::node_graph()` (new accessor) with
a `dynamic_cast` every frame, because loading a preset replaces the graph and every node in
it. It renders nothing when the active graph has no MPM node, so it only appears for the
MLS-MPM preset.

Play/Pause was *removed* from the node renderer so there is exactly one animation driver;
Step and Reset are one-shot and stayed. Registered under `ALP_WEBGPU_APP_ENABLE_COMPUTE`
right after `NodeGraphPanel`, whose pointer it holds.
