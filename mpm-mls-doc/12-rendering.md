# 12 — Rendering the avalanche in 3D

How the MPM avalanche is drawn on the map, and where each part lives. The background research
with sources is
[Avalanche Particle Rendering](https://claude.ai/artifact/PWiuxTHUn9LvobGEQpksXj) (claude.ai,
private), linked from the Notion page "Rendering MPM Particles and Avalanche Surfaces".

**Scope:** map mode only. Only the map graph (`mpm_avalanche_simulation.json`) has the
"Particles 3D" node, so the 3D controls appear only there. The sandbox keeps its 2D density
view and map overlay, because its particles sit at AvaFrame's DEM altitudes, not on weBIGeo's
terrain. In map mode the solver runs on weBIGeo's own height texture, so the particles lie
exactly on the rendered terrain.

## What the map shows

By default, all lit like the terrain:

| Layer | What it is | Default |
|---|---|---|
| **Flow surface** | A snow blanket over the dense flow, from a B-spline mass splat on a render grid 3× finer than the solver's. | on |
| **Track** | A thin, duller layer wherever the flow has been since the release. | on |
| **Entrainable snow** | With entrainment: the cover the flow can still pick up, a layer of fresh, slightly blue snow that ends where the flow has eroded it. | on |
| **Snow cover** | Lit snow on the terrain around the avalanche, thinning to sparse patches below a snow line fitted to the terrain. | on |
| **Particles** | Powder: soft, cloudy, transparent blobs over the surface. Spheres and strain-rate ellipsoids are selectable instead. | Powder |

The 2D density overlay (`mpm_splat.wgsl` via `OverlayRenderNode`) is hidden while "3D
particles" is on and comes back when it is off.

## Files

| File | Role |
|---|---|
| `webgpu/compute/nodes/MpmSolverNode.h` | `MpmParticleView`, the solver's output socket "particle view": particle buffer, count, previous particles and interpolation, metres → world affine, particle spacing, grid columns and their `column_stats` (entrainment cover), height texture, region size, `reset_count`. |
| `apps/webgpu_app/compute/ParticleRenderNode.h/.cpp` | Terminal node "Particles 3D", registered in `RenderingContext.cpp`. Hands the particle view to the renderer, serialises the renderer settings, owns the "Avalanche snow cover" overlay (`sync_snow_cover()`). |
| `webgpu/engine/avalanche/AvalancheRenderer.h/.cpp` | The renderer, owned by the engine `Context`. `Settings`, `Frame`, `prepare()` (compute), `draw()` (G-buffer), `draw_transparent()` (powder). |
| `webgpu/engine/Window.cpp` (`Window::paint`) | Calls `prepare()` before the G-buffer pass, `draw()` inside it after the tiles, and `draw_transparent()` after compose. |
| `webgpu/engine/shaders/render_avalanche.wgsl` | Solid particles: sphere and ellipsoid impostors, detail particles. |
| `webgpu/engine/shaders/render_avalanche_powder.wgsl` | Powder blobs, blended after compose. |
| `webgpu/engine/shaders/avalanche_surface_splat.wgsl` | Flow surface, pass 1: particles → render grid accumulators. |
| `webgpu/engine/shaders/avalanche_surface_field.wgsl` | Flow surface, pass 2: accumulators → depth, height, speed, peak depth (track); the entrainment cover from the solver's column stats. |
| `webgpu/engine/shaders/render_avalanche_surface.wgsl` | Flow surface, pass 3: the blanket, track and entrainable snow, one quad per render cell. |
| `webgpu/engine/shaders/avalanche_surface_common.wgsl` | Shared layouts (`SurfaceConfig`) and `AVALANCHE_MARKER`. |
| `webgpu/engine/shaders/avalanche_surface_terrain.wgsl` | The solver's terrain texture, sampled Catmull-Rom. |
| `webgpu/engine/overlay/ScreenSpaceSnowOverlay.h/.cpp`, `shaders/overlays/screen_space_snow.wgsl` | The snow overlay; skips avalanche pixels; `patches` setting. |
| `webgpu/engine/overlay/Overlay.h`, `OverlayRenderer.cpp` | The `enabled` flag that hides the 2D overlay. |
| `apps/webgpu_app/avalanche/AvalanchePanel.cpp` | The 3D view controls, "Look at the flow", hiding the 2D overlay, calling `sync_snow_cover()` every frame. |
| `mpm-mls-doc/scripts/validate_wgsl.py` | Also validates the engine shaders. |

The shader table with stages and bindings is in [03-shaders.md](03-shaders.md).

## The frame

`Window::paint` renders deferred:

1. Atmosphere.
2. `AvalancheRenderer::prepare()`: the flow surface's compute passes (splat, field).
3. **G-buffer pass:** tiles, then `AvalancheRenderer::draw()` (solid particles and the flow
   surface). The G-buffer holds albedo as R32Uint (`pack4x8unorm`), camera-relative position
   as RGBA32Float, the normal as RG16Uint (octahedral) and an overlay slot as R32Uint, with
   Depth24Plus depth.
4. Clouds.
5. Overlays (including the avalanche snow cover).
6. **Compose:** lights, shadows and hazes everything in the G-buffer. The avalanche gets the
   terrain's lighting and atmosphere for free.
7. `AvalancheRenderer::draw_transparent()`: powder, over the lit frame.
8. Track lines.

Opaque avalanche parts go into the G-buffer. Powder is transparent, so it needs a pass of its
own after compose.

## From the solver to the renderer

- **The particle view.** `MpmSolverNode` exposes `MpmParticleView` and keeps it current
  across buffer reallocations.
- **The source callback.** `ParticleRenderNode` sets an owner-tagged `Source` on the renderer.
  The callback reads the node's input socket every frame, so a reallocated buffer, or a solver
  that is disconnected or removed, is picked up at once. `clear_source(owner)` only clears its
  own source, so a stale node cannot remove a newer one.
- **Placement.** World xy = `world_from_metres` × (x, y, 1). On the map that is the region
  offset and 1/cos(latitude). World z = altitude / cos(latitude), as in `render_tiles`. The
  renderer computes the camera-relative offset in double, because world coordinates are around
  1e6.
- **Interpolation.** In real-time playback the solver keeps the particles from before its
  last chunk (`previous_particles`) plus a blend factor (`interpolation`). Every shader that
  reads particles (solid, powder, splat) draws `mix(previous.xyz, current.xyz, interpolation)`.
  Without previous particles the current buffer is bound in their place and the factor is 1.
  A slot with mass 0 in the previous particles is drawn at its current position. This is due to
  entrainment: a pool slot that was filled during the last chunk was empty before, at the
  domain origin and altitude 0, and would otherwise travel in from there under the terrain.
  See [13-simulation-driver.md](13-simulation-driver.md).

## Solid particles (`render_avalanche.wgsl`)

- **Impostors:** instanced camera-facing quads with vertex pulling from the particle buffer.
  The fragment shader intersects the ray with the sphere and writes exact depth (reverse-Z),
  packed albedo and normal. It forms the distance to the ray directly, against float
  cancellation at km distances.
- **Size:** radius = "Particle size" × particle spacing (default 0.6), with a minimum of
  1.5 px.
- **Colour:** Snow, or Speed (viridis from 0 to 60 m/s).
- **Strain-rate ellipsoids:** the shape is I + window · D, where D is the symmetric part of
  the particle's APIC velocity gradient C. The window is shortened so no axis changes by more
  than 70 %, and the volume is normalised. The ray is solved in the ellipsoid's unit-sphere
  space. "Strain window" sets the window (default 0.15 s).
  - **Why not F:** the solver keeps only the elastic part of F, which stays within about 1 %
    for snow (Stomakhin clamps it to −2.5 % / +0.75 %; Cam Clay sees kPa stresses against
    E = 3 MPa). The total deformation of a flow grows without bound. So the ellipsoid shows
    what the flow does *now*: stretched where it shears or stretches, flattened where it
    compacts.
- **Detail particles** (rendering only; 1, 8 or 27, default 8): each simulated particle is
  drawn as K smaller ones.
  - Instance i is child i mod K of parent i / K.
  - The offsets are a hash of (parent, child), stable every frame and moving with the parent.
    Each is a uniform point in the unit ball, stretched by the strain-rate shape and scattered
    within 2 parent radii (`CHILD_SPREAD`).
  - Size: radius × K^(-1/3) × a jitter in 0.7–1.3, so the volume is kept on average.
  - Below 1.5–3 px parent radius the children merge back into the parent
    (`CHILD_FADE_PIXELS`), where they would only flicker.
  - Packing the children into the parent's own sphere looked like a lumpy clod, and using the
    parent's volume-equivalent ball made the clusters smaller than single particles. Hence
    the 2-radius spread.

## Powder (`render_avalanche_powder.wgsl`)

The default particle shape. The particles act as a guide over the flow surface, which carries
the mass.

- **Look:** each particle is a large, soft, transparent blob. Its radius is "Powder size" ×
  spacing (default 8, range 1–10) and its peak alpha is "Powder opacity" (default 0.3).
  Falloff exp(−3r²)·(1−r²).
- **Noise:** 3-octave value noise across each blob at a per-particle hashed offset. Every blob
  gets its own fixed cloudy pattern, computed in the shader with no texture.
- **Blending:** premultiplied "over", no depth attachment, no sort. All blobs share one colour,
  so the order does not change the result.
- **Soft intersection:** the alpha is scaled by the share of the ray's path through the blob
  that lies in front of the G-buffer depth, linearised with `inv_proj_matrix`. Blobs sink
  softly into the terrain and the flow surface instead of being cut off.
- **Lighting:** ambient + sun from the shared config, wrapped diffuse on the sphere normal for
  a translucent look. The atmosphere's transmittance and inscatter are computed per vertex.
- In powder mode the solid particles, detail particles and strain window are unused. The flow
  surface still uses the solid radius ("Particle size").

## Flow surface (`avalanche_surface_*.wgsl`, `render_avalanche_surface.wgsl`)

Three passes, the same structure Stomakhin 2013 uses to rasterise for its renderer:

1. **Splat** (compute). Each particle's volume (release volume / N = spacing³) goes onto a
   render grid "Surface detail" times finer than the solver's (default 3, e.g. 2.7 m). It uses
   the solver's quadratic B-spline at the solver's dx, so the field is as smooth as what the
   solver resolves. Fixed-point atomics per node: Σw, Σw·speed, Σw·height above the terrain.
   Particles more than 2 dx above the terrain (spray, jumps) are faded out from 1 dx.
2. **Field** (compute, same pass, so it sees the splat).
   - **Depth** = Σw · V / dx², the volume per area at release density. The blanket is drawn
     where depth ≥ min_depth = max(8 particles' volume over 3×3 columns, 5 cm)
     (`surface_min_count`). Stray particles stay particles.
   - **Height:** 2 × the weighted mean particle height + the drawn radius. An evenly filled
     layer has its particles at half its height on average. The moving flow is looser than at
     release, so with the volume depth alone the particles stuck out of a thin blanket.
   - **Edge taper:** the height fades back to the volume depth between 1× and 4× min_depth,
     so the edge meets the ground.
   - **Peak depth** per node (`m_peak`), for the track.
   - **Cover depth:** the entrainment cover still on the ground, `column_stats.cover / dx²`,
     bilinear between the solver's columns. The surface lies on the higher of flow and cover,
     not their sum, because the solver's particles move on the bare terrain.
3. **Draw** (G-buffer). One quad per render cell, rejected in the vertex shader when no corner
   reaches min_depth and no corner has 2 cm of cover (the bilinear interpolant cannot exceed
   its corners). Normals by central differences on the render grid.

The terrain under all of this is the solver's height texture sampled Catmull-Rom: it passes
through the same texel heights as the solver's bilinear lookup, but its slope is continuous.
This is due to the cover layer: a thin, wide layer on bilinear terrain is lit in blocks of the
texture's texels (zoom-15 height tiles, roughly 13 m on the map), which the flow's own
relief had hidden.

Every step is continuous in the particle positions, so there is no frame-to-frame popping and
no temporal smoothing (which would add lag).

This replaces a simpler per-column max binning on the solver grid, because that flickered (a
particle crossing a column border jumps the top altitude), aliased at 8 m and depended on a
count threshold. An earlier variant of it placed edge vertices at the lowest neighbouring
particle and cut into slopes.

## Track, entrainable snow and snow cover

The track and the snow cover keep the path and the surroundings readable where the flow
surface has holes; the entrainable snow shows what the flow will pick up. The first two are
needed because behind bumps the flow itself splits into streaks and leaves gaps with no particle
within the kernel's reach (12 m): on a paused frame with the airborne cut-off off and a
1-particle threshold, the holes stay, so tuning the surface cannot fix them.

- **Track** (`Settings::track`). The field pass keeps a persistent peak depth per render node,
  the same quantity as AvaFrame's peak flow thickness. It is cleared when the solver's
  `reset_count` changes. Where peak ≥ min_depth, a 0.3 m layer stays (tapered to half towards
  its edge), drawn with `TRACK_ALBEDO` (0.80 / 0.82 / 0.86, scoured and compacted). The
  blanket's rim blends into that colour from 1× to 3× min_depth, which also outlines it.
- **Entrainable snow** (`Settings::entrainment_cover`). The cover depth of the field pass,
  drawn where it is at least 2 cm (`COVER_MIN_DEPTH`) with `COVER_ALBEDO` (0.85 / 0.91 / 1.0,
  a little blue to stand apart from the white snow overlay), in speed colours a pale blue.
  Order: flow over track over cover, so the cover ends where the flow has been. The solver
  keeps the cover as one number per grid column (15-entrainment.md), not as particles, and
  fills pool particles at rest under the flow when it erodes it. Without this layer nothing
  marks what the flow will pick up, and the picked-up snow is indistinguishable from the rest.
  The renderer reads `column_stats` straight from the solver (`Frame::column_stats`); with
  entrainment off the cover is 0 and nothing is drawn.
- **Snow cover** (`Settings::snow_cover`). `ParticleRenderNode::sync_snow_cover()`, called by
  the panel every frame, adds an "Avalanche snow cover" `ScreenSpaceSnowOverlay` on first use
  and removes it when the node is destroyed.
  - It sits in the pre-shading bucket (z_index below all others), so it is lit like the
    terrain.
  - **Avalanche marker:** all avalanche fragments write `AVALANCHE_MARKER` (0x00FFFFFF, alpha 0
    so tile-debug ignores it) into the G-buffer overlay slot. `screen_space_snow.wgsl` binds
    that slot and skips those pixels. The user's own snow overlays skip the avalanche too.
  - **Snow line fitted to the terrain:** from the solver's terrain scan. Full cover above 55 %
    of the relief (release areas and track); the covered share reaches 0 at 15 % of the relief
    below the lowest point; the line varies by 8 % of the relief. Up to 50°. Refitted when the
    terrain range changes (a new scenario); the user's changes in the Overlays panel hold until
    then.
  - **Patches** (overlay setting, 1 for the avalanche cover, 0 elsewhere by default): the
    falloff becomes the *share of ground* covered. A pixel is snowy where world-space noise
    (two octaves, 40 world units) lies below it, so thin snow is sparse, irregular patches
    instead of a grey veil. Where a noise cell is only a few pixels across it falls back to the
    average, so there is no shimmer.

The marker and the pre-shading cover are there because a user snow overlay otherwise paints
the avalanche white on white, flat and unlit (it composites after lighting by default). User
overlays stay where they are in the Overlays panel.

## Panel controls (Avalanche panel, map mode)

| Control | Setting | Default |
|---|---|---|
| 3D particles + colour (Snow / Speed) | `enabled`, `colour_mode` | on, Snow |
| Flow surface | `surface` | on |
| Look at the flow | camera 400 m from the centre of mass, steeply from the south | — |
| Track | `track` | on |
| Entrainable snow | `entrainment_cover` | on |
| Snow cover | `snow_cover` | on |
| Surface detail (1–4× the grid) | `surface_refine` | 3 |
| Particle shape (Spheres / Strain-rate ellipsoids / Powder) | `shape` | Powder |
| Powder size, Powder opacity | `powder_size`, `powder_opacity` | 8, 0.3 |
| Particle size, Detail particles | `radius_scale`, `children` | 0.6, 8 |
| Strain window | `strain_window` | 0.15 s |

The settings live in `AvalancheRenderer::Settings` and are serialised by `ParticleRenderNode`.

## Limits

- **Not measured:** the GPU cost. The heavy parts are detail particles (8 × 65 536 quads),
  the surface at 3× (Medium grid: 1197² nodes, about 1.4 M quads, most rejected in the vertex
  shader, plus up to 10×10 nodes per particle in the splat), and powder overdraw (it grows
  with the square of "Powder size").
- **Web build:** tested in the browser by Manuel (25 Oct); it renders as natively.
- **Checked on Hintertux only** for detail particles, the mass-splat surface, track, snow cover
  and powder. The checks are listed in [06-verification.md](06-verification.md).
- The track is one flat colour, with no streaks along the flow.
- The entrainable snow is drawn on the solver's terrain (roughly 13 m texels on the map), so it
  smooths away the finer relief of the map tiles under it, and its edge follows the solver's
  columns (8.8 m), not the outline of the entrainment area. It was checked on Madlein (avaMal,
  AvaFrame's entrainment area) only; a generic map scenario covers everything up to 50°.
- In sparse spray, clusters of 8 detail particles are still visible as groups.

## Not chosen

- **Marching cubes:** the band grid is too coarse for a 1–5 m flow, and a finer grid every
  frame is heavy.
- **Screen-space triangulation:** noisy on a moving cloud.
- **Screen-space fluid rendering:** looks liquid and is view-dependent. Still an option for
  smoothing.
- **Further ideas** (a ray-marched grid volume for powder, and others): the Notion page
  "Rendering — future work".
