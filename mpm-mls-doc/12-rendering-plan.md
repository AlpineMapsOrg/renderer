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
3. **Strain-rate ellipsoids.** Done 2026-10-18. The plan said "ellipsoids from F", as
   PhysGaussian does, but the solver stores only the *elastic* part of F, and that stays within
   about 1 % for snow:
   - Stomakhin clamps it to −2.5 % / +0.75 %.
   - Cam Clay sees kPa stresses against E = 3 MPa.
   - The total deformation of a flow grows without bound.

   So the ellipsoid comes from the particle's current strain rate instead: the symmetric part D
   of its APIC velocity gradient C. Shape = I + window · D, with the window shortened so no axis
   changes by more than 70 %, and the volume normalised.
   - The ray is solved in the ellipsoid's unit-sphere space.
   - Panel: "Particle shape" (spheres / strain-rate ellipsoids) and "Strain window".
   - **New button, "Look at the flow":** puts the camera 400 m from the centre of mass. It
     needs the correct world z, altitude / cos(latitude).

   **Verified in map mode** (Hintertux, 77 s): moving particles show as ellipsoids stretched
   along the flow, resting ones as spheres. The check used a temporary test build that framed
   the camera automatically, since then removed.
4. **Flow surface.** Done 2026-10-18 (map mode); replaced by step 6 on 2026-10-23. Three passes per frame, all in
   `AvalancheRenderer`:
   - **Bin** (`avalanche_surface_bin.wgsl`, compute, before the G-buffer pass): per solver
     grid column, the highest and lowest particle altitude, the count and the top speed, as
     atomics on a zeroed buffer.
   - **Depth** (`avalanche_surface_depth.wgsl`, compute): flow depth = top particle + radius −
     the solver's own terrain at the column. It is 0 where a column has fewer than
     `surface_min_count` (2) particles. The terrain sampling is shared with the solver's
     (`avalanche_surface_terrain.wgsl`; `MpmParticleView` now carries the height texture and the
     region size).
   - **Draw** (`render_avalanche_surface.wgsl`): one quad per grid cell into the G-buffer.
     - Each vertex sits at terrain + depth smoothed over 3×3 (1-2-1). Empty columns count as 0,
       so the blanket thins towards its edge.
     - Fragments thinner than 5 cm are discarded: the outline is a smooth contour, not the
       grid's squares.
     - Normals come from neighbouring blanket heights.
   - **Panel:** "Flow surface" toggle, on by default.

   **History:** a first version placed edge vertices at the lowest neighbouring particle
   instead of the terrain. It cut into slopes and showed the grid's squares, which is why the
   depth pass exists.

   **Verified in map mode** (Hintertux, 74 s): a continuous lit blanket over the dense core,
   particles above it as front and spray.

   **Fixed the same day:**
   - **Isolated patches:** a column now joins the blanket only where its 3×3 neighbourhood
     holds at least 8 particles (`surface_min_count`). Stray pairs stay particles.
   - **2D density overlay:** hidden while "3D particles" is on.
     - Overlays got an `enabled` flag (`Overlay.h`). `OverlayRenderer` builds its ping-pong
       chain from the enabled ones only.
     - `OverlayRenderNode::set_overlay_enabled` is set by the Avalanche panel each frame in the
       map graph.

   **Verified:** Hintertux at 74 s shows no discs and no scattered patches; one blob remains,
   over a genuinely dense cluster.
5. **Detail particles.** Done 2026-10-23, after the user found the particles too sparse next
   to the blanket. Rendering only: each simulated particle is drawn as K smaller ones (panel
   "Detail particles": 1 / 8 / 27, default 8). Stomakhin 2013 never shows particles at all (it
   rasterises them and path-traces a volume), and Hu 2018 describes no rendering, so this is
   our own choice.
   - **Instances:** particle_count × K. Instance i is child i mod K of parent i / K.
   - **Offsets** are a hash of (parent, child): stable every frame, moving with the parent. Each
     offset is a uniform point in the unit ball, stretched by the strain-rate shape.
   - **Size:** radius × K^(-1/3) × a jitter in 0.7–1.3, so the volume is kept on average.
   - **Spread:** the children are scattered within 2 parent radii (`CHILD_SPREAD`).
     - **First try:** the parent's volume-equivalent ball, 0.62 spacing. The parent is drawn
       at 0.8 spacing, so the clusters looked *smaller* than single particles.
     - **Second try:** packed into the parent's own drawn sphere. The gaps between the
       children made each particle read as a lumpy clod, not as more particles.
   - **Distance fade:** below 1.5–3 px parent radius the children merge back into the parent
     (`CHILD_FADE_PIXELS`), where they would only flicker. The pixel floor applies to the
     parent, not to each child.

   **Verified** (Hintertux, paused at 40 s, camera at 150 m, the same frame switched from 8 to
   1): with 8 the spray reads as many finer grains in loose clusters, not as one ellipsoid each.

   **Limits:**
   - In sparse spray the clusters of 8 are still visible as groups.
   - The detail adds texture but cannot fill the gap between the blanket and the particles;
     that is the flow surface's job (step 6).
   - GPU cost not measured yet: 8 × 65 536 quads.
6. **Mass-splat flow surface.** Done 2026-10-23. It replaces step 4's binning: the user saw
   flicker and 8 m aliasing.

   **Causes in step 4:**
   - Per-column max binning: a particle crossing a column border jumps the top altitude.
   - The count threshold.
   - An 8 m mesh.

   **Now three passes**, the same structure as Stomakhin 2013 rasterising for its renderer:
   - **Splat** (`avalanche_surface_splat.wgsl`): each particle's volume (release volume / N =
     spacing³) goes onto a render grid `surface_refine` times finer than the solver's (default
     3, so 2.7 m). It uses the solver's quadratic B-spline at the solver's dx, so the field is
     as smooth as what the solver resolves. Fixed-point atomics per node: Σw, Σw·speed and
     Σw·height above the terrain.
     - Particles more than 2 dx above the terrain (spray, jumps) are faded out from 1 dx.
   - **Field** (`avalanche_surface_field.wgsl`):
     - **Depth** = Σw · V / dx², the volume per area at release density. It decides where the
       blanket is drawn: depth ≥ min_depth = max(8 particles' volume over 3×3 columns, 5 cm).
     - **Blanket height:** 2 × the weighted mean particle height + the drawn radius. An evenly
       filled layer has its particles at half its height on average. This is needed because
       the moving flow is looser than at release; with the volume depth alone the particles
       stuck out of a thin blanket everywhere.
     - **Edge taper:** the height fades back to the volume depth between 1× and 4× min_depth,
       so the edge meets the ground.
   - **Draw** (`render_avalanche_surface.wgsl`):
     - A quad per render cell. It is rejected when no corner reaches min_depth; the bilinear
       interpolant cannot exceed its corners.
     - Normals by central differences on the render grid.

   Every step is continuous in the particle positions, so there is no frame-to-frame popping
   and no temporal smoothing was needed (it would add lag).

   **Panel:** "Surface detail" 1–4× (the default preset does not store it).

   **Verified** (Hintertux, paused at 40 s, 400 m, the same frame at 3× and 1×):
   - 3× gives a smooth, rounded outline; 1× shows facets.
   - With the height term, the blanket is continuous and lit, with relief, and covers the dense
     core.
   - A burst of frames during playback shows coherent, non-popping change.

   **Not measured:** GPU time. At 3× the Medium grid is 1197² nodes, about 1.4 M quads, most
   rejected in the vertex shader, plus up to 10×10 nodes per particle in the splat.
7. **Gaps over bumps: track and snow cover.** Done 2026-10-23. The user saw bare holes in
   the flow where it passes over bumps.

   **Diagnosis** (Hintertux, paused at 55 s, 800 m; the same frame rendered three ways):
   - Without the airborne cut-off: hardly any change.
   - Also with a threshold of one particle: a few extra blobs where stray particles are.
   - **The holes stay**, so there are no particles within the kernel's reach (12 m) there at all.
     The flow really splits into streaks and leaves gaps behind bumps. The renderer's thresholds
     are not the cause, and tuning them cannot fix it.

   **B, track** (`Settings::track`, on by default):
   - The field pass keeps a persistent peak depth per render node (`m_peak`), the same quantity
     as AvaFrame's peak flow thickness. It is cleared when the solver's `reset_count` changes
     (new in `MpmParticleView`).
   - Where peak ≥ min_depth, a 0.3 m layer stays (tapered to half towards its edge).
   - It is drawn with `TRACK_ALBEDO` (0.80 / 0.82 / 0.86, scoured and compacted). The blanket's
     rim blends into that colour from 1× to 3× min_depth, which also outlines it.

   **A, snow cover** (`Settings::snow_cover`, on by default):
   - `ParticleRenderNode::sync_snow_cover()`, called by the Avalanche panel every frame, adds an
     "Avalanche snow cover" `ScreenSpaceSnowOverlay` on first use. It sits in the pre-shading
     bucket (z_index below all others), so it is lit like the terrain.
     - Settings: up to 50°, snow line 800 m. Both can be changed in the Overlays panel.
   - The node removes the overlay when it is destroyed (another preset).
   - All avalanche fragments write `AVALANCHE_MARKER` (0x00FFFFFF, alpha 0, so tile-debug ignores
     it) into the G-buffer overlay slot. `screen_space_snow.wgsl` skips those pixels; it now
     binds that slot.
   - Fixes 1 and 3 of the snow-overlay section below; the track and rim cover part of fix 2.

   **Verified** on the same paused frame, three ways (snow + track / track only / neither):
   - The hill is white and lit.
   - Steep forest stays dark.
   - The path reads as a grey band.
   - The debris stays bright with relief on top.

   **Open:**
   - The track is one flat colour (no streaks along the flow).
   - The snow line is fixed at 800 m, not per scenario.
   - GPU time not measured.
8. **Powder particles.** Done 2026-10-23, the user's idea: particles as gas, a guide over
   the surface, which carries the mass. It is a third "Particle shape", Powder.
   - **Look:** each particle is a large, soft, transparent blob, default radius 8× spacing
     (panel "Powder size" 1–10) and peak alpha 0.3 ("Powder opacity"). The falloff is
     exp(−3r²)·(1−r²).
   - **Noise (user's suggestion):** value noise across each blob, 3 octaves, at a per-particle
     hashed offset. Every blob gets its own fixed cloudy pattern; it is computed in the shader,
     with no texture asset.
   - **Pass:** `render_avalanche_powder.wgsl` runs in a pass of its own after compose, like the
     track lines (`AvalancheRenderer::draw_transparent`, called from `Window::paint`). It blends
     premultiplied "over" with no sort: all blobs share one colour, so the order does not change
     the result.
   - **Soft intersection:** the alpha is scaled by the share of the ray's path through the blob
     that lies in front of the G-buffer depth, linearised with `inv_proj_matrix`. Blobs sink
     softly into the terrain and the flow surface instead of being cut off.
   - **Lighting:** ambient + sun from the shared config, with wrapped diffuse on the sphere
     normal for a translucent look. The atmosphere's transmittance and inscatter are computed
     per vertex.
   - **Other settings:** in Powder mode the G-buffer particles, detail particles and strain
     window are not used. The flow surface keeps the solid radius (Particle size).
   - **Verified** (Hintertux, paused at 55 s, 800 m, the same frame): powder at 3× and 6×, and
     ellipsoids. The point-like particles become a soft, cloudy haze over the flow, densest where
     it is thick.
   - **Not measured:** GPU cost. 65k blobs × many pixels of overdraw; Powder size scales it
     quadratically.
9. **Optional / future:** a ray-marched grid volume for powder, screen-space fluid rendering and
   others. Collected on the Notion page "Rendering — future work".

**Verification for each step:** screenshots on the map and in the sandbox, GPU time from
timestamp queries, and the web build. The 2D raster stays the default until the 3D view is
verified.

## With the screen-space snow overlay (checked 2026-10-18)

`ScreenSpaceSnowOverlay` paints snow by slope (0–45°) and altitude (above a snow line), from
the G-buffer position and normal only.

**With it on, the 3D avalanche does not fit in yet** (Hintertux; screenshots in the report):
- **Default snow line, 1000 m:** the whole terrain is flat white, the avalanche included. It
  cannot be seen.
- **Snow line 2000 m:** below it the avalanche reads well. In the snowy track it nearly
  disappears, and it looks greyer than the flat-white overlay snow.

**Causes:**
1. The overlay paints our G-buffer pixels too; it cannot tell terrain from avalanche.
2. White on white has no contrast.
3. The overlay is composited after lighting, so it is flat and unlit, while the avalanche is
   lit.

**Proposed fixes:**
1. A marker in the G-buffer overlay slot for avalanche pixels, which the snow (and height-line)
   overlays skip.
2. A cool tint plus a silhouette outline on snow, or speed colours while the overlay is on.
3. Composite the snow overlay before lighting.

Fixes 1 and 3 are implemented by the avalanche's own snow cover (step 7). The user's own
snow overlays also skip the avalanche now, but stay wherever they are in the Overlays panel
(post-shading by default, so unlit).

## Not chosen now

- **Marching cubes:** the band grid is too coarse for a 1–5 m flow, and a finer grid every frame
  is heavy.
- **Screen-space triangulation:** noisy on a moving cloud.
- **Screen-space fluid rendering:** looks liquid and is view-dependent. It stays an option for
  smoothing.
