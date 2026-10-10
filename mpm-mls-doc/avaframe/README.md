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

## Not done yet

- **Overlay alignment:** checked only numerically (the corner transform and the preview
  against AvaFrame's own plots). It has not yet been checked on the map in the app.
- GeoTIFF DEMs.
- Entrainment and resistance benchmarks. They are ignored on purpose: the MPM solver has
  neither.
