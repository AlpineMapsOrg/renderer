# 12 — Rendering the avalanche in 3D (plan)

Branch `worktree-mpm-rendering`. Full research report with sources:
[Avalanche Particle Rendering](https://claude.ai/artifact/PWiuxTHUn9LvobGEQpksXj) (claude.ai, private),
linked from the Notion page "Rendering MPM Particles and Avalanche Surfaces".

## Starting point

- **Today:** the solver shows a top-down 2D density raster (`mpm_splat.wgsl`), an overlay via
  `OverlayRenderNode`.
- **Particles:** 32–131 k at 128 B each. Every particle carries position, velocity, the elastic
  deformation gradient F, plastic state and stress, already in a GPU buffer.
- **Spacing and cells:** particles are 0.7–1.9 m apart; grid cells are 5–12.5 m. The flow is a
  1–5 m sheet over 1–3 km.
- **The frame** (`Window::paint`): atmosphere → G-buffer pass (tiles) → clouds → overlays → compose
  → forward pass (tracks).
  - The G-buffer holds albedo as R32Uint (`pack4x8unorm`), camera-relative position as
    RGBA32Float, the normal as RG16Uint (octahedral) and an overlay slot as R32Uint, with
    Depth24Plus depth.
  - Anything written there is lit, hazed and cloud-shadowed by the compose pass for free.
  - Overlays are screen-space only.
  - Transparent effects need a forward pass after compose.

## Decision

A hybrid:
- **Front, spray and breaking slabs:** ellipsoid particles from F.
- **Dense core:** a draped flow surface aggregated per grid column.
- **Later:** transparent powder.

Everything opaque goes into the G-buffer.

## Status (2026-10-18)

Steps 1 and 2 are implemented and run on macOS.

**Target: map mode.** Only the map graph (`mpm_avalanche_simulation.json`) has the "Particles 3D"
node, so the panel controls appear only there. The sandbox keeps its 2D view and map overlay:
its particles sit at AvaFrame's DEM altitudes, not on weBIGeo's terrain. In map mode the solver
runs on weBIGeo's own height texture, so the particles lie exactly on the rendered terrain.

**Verified in map mode** (Hintertux scenario, 132 s): the lit spheres show the deposit lobe in
the valley, the remaining snow at the release and scattered particles along the path, with the
haze on them like on the terrain.
- The automated run used a local test build without the first-run notice, which otherwise
  covers the view. The preset was restored and rebuilt afterwards.
- An earlier sandbox batch run (avaKot, 25 s) also drew correctly.

**The pieces:**
- `MpmParticleView` in `MpmSolverNode.h`: an output socket "particle view" holding the buffer,
  count, metres → world affine and particle spacing. Kept current across reallocations.
- `ParticleRenderNode` (`apps/webgpu_app/compute/`), registered in `RenderingContext`. It sets
  an owner-tagged source callback on the renderer, re-resolved through its input socket every
  frame.
- `AvalancheRenderer` (`webgpu/engine/avalanche/`) and `render_avalanche.wgsl`: sphere impostors
  in the G-buffer pass after the tiles.
  - Vertex pulling from the particle buffer.
  - World z = altitude / cos(latitude), as in `render_tiles`.
  - A minimum pixel radius.
  - The ray–sphere test forms the distance to the ray directly, against float cancellation
    at km distances.
  - `frag_depth` is written with the reverse-Z projection.
- The map graph preset has a "Particles 3D" node. The Avalanche panel has a "3D particles"
  toggle, colour (snow / speed) and size.
- `validate_wgsl.py` now also checks engine shaders (`render_avalanche`).

**Not verified yet:**
- The web build.
- A close-up from a low camera; the automated views are from the scenario's framing, about 5 km away.

## Steps

1. **Plumbing.**
   - An `AvalancheRenderer` in `webgpu/engine/avalanche/`, registered in `Context` and drawn
     in the G-buffer pass of `Window::paint`.
   - An app-side node, like `OverlayRenderNode`, links the solver's particle buffer and the
     transform from solver metres to world coordinates: map mode by latitude scale, sandbox by
     `SandboxTerrain::sandbox_to_world`.
2. **Sphere impostors.**
   - Instanced quads with vertex pulling from the particle buffer; ray-sphere in the fragment
     shader, with exact depth, packed albedo and normal.
   - Radius from the particle volume, with a minimum pixel size.
   - Colour modes: white, speed, plastic state, release.
3. **Ellipsoids from F**, as PhysGaussian does: covariance = F · rest · Fᵀ, with the stretch
   clamped.
4. **Flow surface.**
   - A compute pass per solver chunk: the top particle height above the terrain per column
     (atomic max), smoothing, normals.
   - A grid mesh over the domain, drawn into the G-buffer.
   - Particles above the surface stay visible as spray.
5. **Optional:** transparent splats or a ray-marched grid volume for powder; screen-space depth
   smoothing.

**Verification for each step:** screenshots on the map and in the sandbox, GPU time from
timestamp queries, and the web build. The 2D raster stays the default until the 3D view is
verified.

## Not chosen now

- **Marching cubes:** the band grid is too coarse for a 1–5 m flow, and a finer grid every frame
  is heavy.
- **Screen-space triangulation:** noisy on a moving cloud.
- **Screen-space fluid rendering:** looks liquid and is view-dependent. It stays an option for
  smoothing.
