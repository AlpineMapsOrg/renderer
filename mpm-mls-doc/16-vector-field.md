# 16 — Vector field for flow visualisation

The avalanche models hand a flow field to whatever draws or exports it: a flow vector and a few
attributes per cell of a regular 2D grid, on the GPU, in one neutral format. The MLS-MPM solver
delivers it through a baseline builder (`MpmBaselineFieldBuilder`). The trajectory model does not
yet. Drawing the field (streamlines, arrows) is not part of this; it is what the format is for.

The format and its interfaces are `webgpu/compute/VectorField.h` and
`webgpu/compute/shaders/vector_field.wgsl`.

## Why a neutral format

A model's own data is not a good thing to read from the outside. The MPM grid is three-dimensional,
stored in a band of layers that follows the terrain, rewritten every substep, and its layout
changes as the solver is optimised. The trajectory model stores scalar maxima and counts per cell.
A reader that depended on either would break with every change in the model, and would need to know
two layouts. So each model converts its own data into the same record, and a reader depends on
that record only.

This is also why it is not a graph socket of the node editor. The graph runs on demand, and the
solver is advanced every frame by the simulation driver ([13-simulation-driver.md](13-simulation-driver.md)),
with positions interpolated between states. A field behind a socket would change once per
completed run and would carry no time, no georeference and no previous state.

## Two layers

- **The baseline.** Every model has a `FieldBuilder` that turns its raw data into the record, and
  a test that pins it. It always works with the current model, so a reader can rely on it.
- **Experiments.** Any number of other `FieldBuilder`s read a model's raw view
  (`VectorFieldSource::raw()`) and build their own field: another weighting, a depth range,
  smoothing, a finer grid from the particles. A builder may append channels to the record, and the
  baseline channels stay first. Builders are chosen at run time, so an experiment can be compared
  with the baseline on the same running avalanche. They are the reader's own code and are free to
  change; they may break when a raw view's `layout_version` changes.

## The record

`FieldRecord` in `vector_field.wgsl`, 8 words (32 bytes) per cell. Cell `(x, y)` is at index
`y · resolution.x + x`.

| Word | Channel | Unit | Meaning |
|---|---|---|---|
| 0–2 | `velocity` | m/s | Mean flow velocity of the cell; for a depth-resolved model the mass-weighted mean over depth |
| 3 | `depth` | m | Vertical thickness of the snow |
| 4 | `mass` | kg | Snow in the cell |
| 5 | `pressure` | Pa | Basal normal stress |
| 6 | `coherence` | 0–1 | How well the directions in the cell agree (1: all the same); for models of paths, not of a continuum |
| 7 | `flags` | | Which channels are valid: `FIELD_VELOCITY` 1, `FIELD_DEPTH` 2, `FIELD_MASS` 4, `FIELD_PRESSURE` 8, `FIELD_COHERENCE` 16 |

A cell without flow has flags 0 and all channels 0. A channel whose flag is not set is 0.

## The interface

```cpp
struct VectorField {                     // what a reader gets from FieldBuilder::field()
    const RawBuffer<uint32_t>* current;  // one record per cell
    const RawBuffer<uint32_t>* previous; // the field before the last update; == current if none
    float interpolation;                 // 0..1 from previous to current (real-time playback)
    glm::uvec2 resolution;               // cells
    glm::vec2 origin_metres;             // cell (x, y) is sampled at origin + (x, y) · cell_size_m
    float cell_size_m;
    glm::dmat3 world_from_metres;        // the field's metre frame -> weBIGeo world xy: world = M · (x, y, 1)
    uint32_t stride_words;               // 8; larger if a builder appends channels
    std::vector<FieldChannel> channels;  // name, offset, components, unit; the baseline channels first
    std::optional<float> simulated_time; // empty for a static field
    uint64_t generation;                 // changes whenever the contents change
    uint64_t resource_generation;        // changes when the buffers are reallocated: rebuild bind groups
};

class FieldBuilder {                     // raw data -> VectorField
    virtual std::string name() const = 0;
    virtual bool accepts(const RawSource&) const = 0;
    virtual void encode(WGPUCommandEncoder, const RawSource&) = 0;  // records work if the model has new data
    virtual const VectorField* field() const = 0;                   // nullptr while there is none
};

class VectorFieldSource {                // implemented by a model's node, like TimeSteppedSimulation
    virtual RawSource raw() const = 0;
};
```

The velocity is in the field's own metre frame, with z up; the linear part of `world_from_metres`
turns its direction into the map's. Consumers find the models the way `SimulationDriver` finds
simulations: a `dynamic_cast<VectorFieldSource*>` over the graph's nodes.

A reader's frame:

1. Call `builder.encode(encoder, source->raw())` before the frame that reads the field. It encodes
   work only if the model has new data since the last call; otherwise it only updates the
   interpolation and the time.
2. Bind `field()->current` (and `previous` for interpolation) read-only in the reader's own bind
   group. Nothing is copied, and no synchronisation is needed: WebGPU has one queue per device, so
   the reader's pass runs after the solver's chunks.
3. Rebuild the bind groups when `resource_generation` changes.

## The MLS-MPM raw view

`MpmRawView` (in `MpmSolverNode.h`) is what a builder reads from the solver. It is documented and
versioned: a change to the grid or particle layout bumps `LAYOUT_VERSION`, and the baseline builder
is updated with it.

| Field | Meaning |
|---|---|
| `particles` | The solver's particle view: the particle buffers, the grid geometry (`domain_origin`, `dx`, `columns`), `world_from_metres`, the terrain texture, `reset_count` |
| `grid_velocity` | One `vec4f` per grid node: `xyz` the velocity [m/s], `w` the node's mass in units of the normalised particle mass. Zero for nodes without flow |
| `column_floor` | Per column, the first stored layer in cells (`i32`) |
| `grid_resolution` | Columns in x and y, layers per column |
| `kg_per_mass_unit`, `snow_density` | Physical mass of one unit of node mass, and the density. `kg_per_mass_unit` is 0 until the first diagnostics readback of a run has told the real volume of a particle |
| `interpolation`, `simulated_time` | Playback state |
| `generation` | Counts the chunks that advanced the simulation; changes whenever `grid_velocity` does |
| `resource_generation`, `layout_version` | Reallocation counter; layout version |

The grid is layer-major. The node of column `c = y · columns.x + x` in layer `l` is
`grid_velocity[l · columns.x · columns.y + c]`, and its altitude is
`(column_floor[c] + l) · dx` ([04-data-layout.md](04-data-layout.md), terrain-following grid).

## The baseline builder for MLS-MPM

`MpmBaselineFieldBuilder` runs `mpm_vector_field.wgsl`, one thread per column:

```
mass     = Σ over layers of w
velocity = Σ w · xyz / mass                   (mass-weighted mean over depth)
kg       = mass · kg_per_mass_unit
depth    = kg / (snow_density · dx²)
```

- **Fringe columns are empty.** A column with less than `min_mass` in total (default 0.5, half a
  particle's worth) is empty. This is due to the nodes at the edge of the flow: they have almost no
  mass but can have large velocities, because a node's momentum includes the stress impulse of the
  particles. A mass-weighted mean over a column made of such nodes alone would show them.
- **Before the particle volume is known** (`kg_per_mass_unit` is 0) only `velocity` is valid;
  `depth` and `mass` come once the first diagnostics have been read back, within the first run.
- **Pressure and coherence are not provided** by this builder; their flags stay unset.
- **Update protocol.** The builder keeps two output buffers and alternates between them, so
  `previous` is the field of the update before. A new field is encoded only when the raw view's
  `generation` has changed. A reseed (`reset_count`) drops `previous`; a reallocation of the solver's
  buffers rebuilds the builder's bind groups. Without a grid (the solver has not run) there is
  no field.
- **Its own pipeline and bind group.** The solver's bind group already uses all eight storage
  buffers a shader stage may have, so the builder cannot share it. It reads only `grid_velocity`.
  The pipeline is compiled once, at construction; there is no shader hot reload.

### What the solver does for it

Three small changes in the solver make the grid readable as a whole:

- The grid update writes the node's mass into the fourth component of `grid_velocity`. The component
  was unused.
- A tile the flow has left is cleared once. The grid update only processes tiles that P2G flagged
  in the substep, so a tile the flow had left would keep its last velocity and mass. The tile flag has a second
  bit, `WAS_ACTIVE`, set when the grid update processes a tile; a tile with only that bit is
  processed once more, finds no mass and zeroes its nodes ([03-shaders.md](03-shaders.md)).
- A reset clears `grid_velocity`, because the flags are cleared with it and the old tiles would
  never be visited.

G2P reads only nodes in flagged tiles, so none of this changes the physics.

## Verification

- `check_substep.mjs` ([bench/README.md](bench/README.md)): the node mass in `w` sums to the number
  of particles (300.0000 for 300); after the flow has been moved 500 m the sum is still 300, not
  600 (the check fails with the clearing switched off); nodes without mass hold no velocity. The
  whole-kernel errors against the float64 reference are the same as before the change.
- `test_VectorField.cpp` (`unittests_webgpu_compute`, needs a GPU): the real C++ class and WGSL
  kernel on a synthetic grid of 24 × 16 columns with empty, flow and fringe columns, every record
  against a float64 reference; the update protocol (two buffers, a new field only for a new
  generation, interpolation following without one, `previous` dropped on a reset, no field
  without a grid); velocity only while the volume is unknown. An unweighted average and a missing
  fringe threshold are both caught.
- `validate_wgsl.py` includes `mpm_vector_field`.

Not covered: a builder in a running app (there is no reader yet), the interpolation of the field in
playback, and a resize of the solver's grid while a builder is attached.

## Cost

Apple M5, the kernel benchmark ([09-performance-analysis.md](09-performance-analysis.md) §3.5):

- The baseline builder takes 0.36 ms per update at the graph preset's size (65 536 particles,
  320² × 12) and 0.58 ms at 320² × 16. It runs once per update, so in real-time playback once per
  frame while a reader is attached. It is memory-bound; the workgroup shape makes little
  difference. Restricting it to the tiles that hold flow would reduce it.
- The grid update change (mass in `w`, clearing left tiles) is not measurable in the total GPU time
  (three A/B runs against the previous kernels: 0.96–1.05×) and leaves the centre-of-mass trace
  identical.
- Memory: 3.1 MiB per output buffer at 320² columns, two of them (6.3 MiB).

## Limits

- Only the MLS-MPM solver has a baseline builder. The trajectory model would need to accumulate a
  direction per cell (two more accumulators) and a builder for it; `RawSource` is a variant with one
  alternative so far.
- A 2D field averages away the variation of the flow with depth. A reader that needs it reads the
  raw view in an experimental builder.
- No reader exists yet: no adapter node hands the field to a renderer, and nothing draws it.
