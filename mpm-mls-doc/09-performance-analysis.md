# Performance: limits, bottlenecks, leaks, and what was changed

*Analysis and first round of fixes, 2026-09-27. Scope: the MLS-MPM extension only
(`MpmSolverNode`, the `mpm_*.wgsl` kernels, `AvalanchePanel`, `MpmSolverNodeRenderer`, the
graph preset) — the rest of weBIGeo is taken as given, except where it is the reason our code
misbehaves.*

Two pieces of feedback started this:

> *"I wonder if you have a memory issue or something because it seems to get slower and slower
> the more often I run it."*

> *"Ich hab die Demo probiert. Sie läuft, aber es ist bei mir extrem langsam (nach 15 Sekunden
> hat sich die Lawine mal langsam in Bewegung gesetzt). Welche Parameter kann ich am besten
> ändern um die Geschwindigkeit zu erhöhen?"*

## 0. Summary

**"Extremely slow, moves after 15 s"** has three factors that multiply, and only one of them is
GPU speed:

1. **Physics at map scale.** A slab on a 35° slope with μ = 0.47 accelerates at
   g(sin 35° − 0.47 cos 35°) ≈ **1.85 m/s²**. After 3 s of *simulated* time it has moved 8 m —
   two texels of the 1024² overlay over 4 km. Visible motion (~50 m) takes **~7 s of simulated
   time**. Real avalanches are like that; on a map they look frozen for the first seconds.
2. **Simulation speed is tied to the frame rate.** The run is fed to the GPU in chunks of
   `substeps_per_submit` = 2 substeps, and a new chunk is only submitted from the previous one's
   work-done callback, which fires from the per-frame event pump. The ceiling is roughly
   `2 chunks × 2 substeps × 0.01 s × fps` = 2.4× real time at 60 fps, 1.2× at 30 fps — before
   the GPU has done any work. On the web, callbacks tend to arrive a frame later still.
3. **GPU cost per substep.** On the Apple M5 the old preset took ~4 ms per substep (100 ms per
   24-substep run, 08-domain-size-options.md §5). A GPU 3–5× slower (integrated Intel, base M1,
   a 4 GB laptop part rendering the terrain at the same time) spends 12–20 ms per substep, so
   2 substeps per frame already halve the frame rate, which halves factor 2 again.

At 0.5× real time, factor 1's 7 simulated seconds become **~15 s of wall time** — exactly the
report. The answer to *"which parameters?"* is therefore: fewer particles, a larger `dt` where
the material allows it, and more substeps per frame (§1.3). This branch makes the last one
automatic and makes each substep cheaper.

**"Slower and slower"**: there were real leaks and lifetime bugs, all small per event and all
fixed here (§5): a leaked command buffer + queue reference and a fresh staging buffer on
*every* run, a pipeline factory with a dangling `this` left behind on *every* graph load, an
unbounded energy-line history copied every frame, and async callbacks that could fire into a
destroyed node. None of them is big enough on its own to explain a clearly visible slowdown
within minutes; the likely co-culprits outside our code are per-run console logging
(4 lines per run on the web, retained by DevTools), VRAM oversubscription on 4 GB cards, and
thermal throttling on fanless laptops. §7 gives a protocol that separates these.

**What was changed** (details in §9):

| Change | Effect (SwiftShader, same scenario) |
|---|---|
| Grid update only over tiles that hold snow; accumulators zeroed there; no clear pass | grid passes −72 % (work ∝ snow footprint, not domain) |
| Stress computed in G2P from the return mapping's SVD; P2G has no SVD | P2G −23 %; 1 SVD per particle and substep instead of 2 |
| *both kernel changes together, 131k particles* | ***1.40× per run, COM trace identical to 2 × 10⁻⁵*** |
| Default detail 131k → 65k particles, 16 → 12 layers | still ~330 particles per cell at release; final position moves 4 cm |
| *new kernels at the new default detail vs the old preset* | ***2.68× per run*** |
| Preset `dt` 0.01 → 0.02 (Stomakhin; verified up to 0.03) | 2× simulated time per substep, trajectory within 0.6 % |
| Adaptive pacing from measured GPU ms per substep | uses the headroom on fast GPUs, protects the frame rate on slow ones |
| Leak / lifetime fixes, memory clamps, GPU readout | see §5 |
| Standalone benchmark, headless A/B against any revision | `mpm-mls-doc/bench/` |

Taken together, the default scenario now needs roughly **5× less GPU time per simulated
second** than before (2.68× cheaper substeps × 2× longer substeps), measured on the CPU
backend; §8 has the numbers and caveats, and a real-GPU run of the benchmark is the next thing
to do (§7).

---

## 1. The "extremely slow" report, in detail

### 1.1 How a run reaches the GPU

```
every frame (App::poll_events → ImGui → AvalanchePanel::draw)
  if playing and solver idle: solver->rerun()
      run_impl(): ensure buffers, write uniform, bind group        (µs of CPU)
      submit_chunk() ×2                                              two chunks in flight
  ...
  wgpuInstanceProcessEvents / device tick  →  work-done callback of chunk k
      submit_chunk()   (chunk k+2)       or, after the last one:
      read_back_state(); complete_run()  →  Overlay node runs → request_redraw
```

The CPU never waits. What the user experiences as "blocking" is the **single GPU queue**: the
frame's render commands execute after whatever solver chunks were submitted before them. The
chunking of commit 9fd10f0 bounds that wait to one chunk per frame, which is right, but it
also turns the frame rate into the simulation's clock.

### 1.2 The throughput ceiling

With two chunks in flight and one callback per chunk per frame:

```
simulated s per wall s  ≤  2 × substeps_per_submit × dt × fps     (callback-bound)
                        ≤  substeps_per_submit × dt / (GPU ms per chunk + render ms)   (GPU-bound)
```

| Machine / situation | fps while playing | ceiling (2 substeps, dt 0.01) |
|---|---|---|
| M5, preset (measured, 05-tuning.md) | 60 | 2.4× (measured 1.65×: run boundaries, readback, overlay) |
| Mid laptop GPU, terrain ~15 ms/frame, 2 × 8 ms chunks | ~30 | ≤ 1.2×, GPU-bound ~0.6× |
| Integrated GPU, 2 × 20 ms chunks | ~15 | ~0.3× |

The last row is the reporter's 15 seconds.

### 1.3 Which parameters make it faster (answer to the question)

In order of effect per unit of lost quality:

| Knob | Why it helps | Cost |
|---|---|---|
| **Detail → Low / Medium** (particles 32k / 64k) | P2G and G2P scale linearly with particles, and at 12.5 m cells even 32k particles are ~160 per cell — Li et al. 2021 use 8. | Sparser rendered cloud; mechanics barely change at this `dx` |
| **Pacing → Fast** (or Manual: *Substeps per frame* 4–8) | Removes the frame-rate ceiling of §1.2 | Lower fps while playing |
| **`dt`** (Stomakhin: 0.02–0.03 verified, §8.4; the preset now uses 0.02) | Simulated time per substep scales with `dt`, cost does not | Stiff presets (Li 2021, E = 3 MPa) are pulled under a conservative bound; §8.4 suggests it could be doubled |
| **Grid layers** 16 → 10–12 | Grid work ∝ layers | Pile headroom `dx × (layers − 4.5)`; 10 layers = 69 m at 12.5 m |
| Output resolution 1024 → 512 | Splat + rasterise once per run | Blurrier overlay; small effect |

Not helpful: `substeps_per_run` (only sets how often the overlay and diagnostics update) and
the domain size (the grid work now follows the snow, §9.1).

---

## 2. Where the GPU time goes

### 2.1 Work per substep

Old kernels (v1), old preset: N = 131 072 particles, 320 × 320 × 16 = 1.64 M grid nodes.

| Stage | Threads | Per thread | Per substep |
|---|---|---|---|
| clear | 1.64 M nodes | 5 atomic stores (20 B) | 33 MB written |
| P2G | 131 k | 128 B load, **1 SVD** (8 Jacobi sweeps), 27 nodes × 4 atomic adds | 14.2 M atomics, 131 k SVDs |
| grid update | 1.64 M nodes | 8 B load; empty → 12 B atomic stores; else momentum → velocity, terrain samples | 1.6 M node visits, ~99 % empty |
| G2P | 131 k | 27 × 3 atomic loads, **1 SVD**, 256 B load/store, terrain samples | 10.6 M atomic loads, 131 k SVDs |

Measured split on SwiftShader (§8.1): **P2G 40 %, G2P 32 %, grid update + clear 27 %**,
splat/rasterise 0.5 %. The M5 fit of 08 §5 gives, for this configuration, ~40 % grid passes and
~50 % particle passes — the grid weighs more on a real GPU, where memory traffic is relatively
more expensive than on a CPU.

### 2.2 The three bottlenecks

1. **P2G atomic contention.** The release disc of the Breite Ries scenario (r = 100 m) covers
   ~200 columns of 12.5 m. 131 072 particles in 200 columns is **~650 particles per cell**; each
   grid node near the slab receives on the order of 10⁴ fixed-point `atomicAdd`s per component
   per substep, all to the same address and therefore serialised in the memory system. Standard
   MPM practice is 8 particles per cell (Li et al. 2021 use exactly that). The particle spacing
   is 0.7 m against 12.5 m cells: the extra particles add almost no mechanical resolution —
   `dx` sets that (05-tuning.md, "the resolution problem") — but they cost linearly and
   contend super-linearly. As the flow spreads the contention eases, which is why P2G gets
   cheaper over a run.
2. **Grid passes over empty air and empty terrain.** Clear + update touch every stored node
   every substep; the snow occupies ~1 % of the columns at release and rarely more than 10–20 %
   of the domain during runout. ~25–45 % of the GPU time went into nodes that were zero before
   and zero after.
3. **Two SVDs per particle and substep.** The stress in P2G re-derived the SVD of the elastic
   deformation gradient that the return mapping in G2P had computed one dispatch earlier.

Secondary: every G2P gather went through `atomicLoad` (the velocities lived in the atomic
accumulator slots), the grid update used 4 × 4 × 4 workgroups over a layer-major layout (poor
coalescing), and the 128 B particle is loaded whole in both particle passes.

### 2.3 What is *not* a bottleneck

- CPU: encoding a chunk is microseconds; there is no CPU readback in the loop other than the
  56-byte diagnostics.
- Splat and rasterise (once per run, < 1 %).
- VRAM capacity (§4).

---

## 3. Defaults and what they imply

| Setting | Header default | Graph preset (before → now) | Scenario override |
|---|---|---|---|
| domain | 1024 m | 4000 m | 4000 m (all three) |
| grid XY | 64 | 320 (dx = 12.5 m) | 320 |
| grid layers | 16 → **12** | 16 → **12** | — |
| particles | 65 536 | 131 072 → **65 536** | — |
| release radius | 120 m | 100 m | 100 / 150 / 130 m |
| slab | 1.5 m | 1.5 m | 1.5 / 2 / 2 m |
| dt | 0.01 s | 0.01 → **0.02 s** | material presets lower it to 0.8 × CFL |
| substeps per run / per frame | 32 / 2 | 24 / 2 | pacing (now automatic) |
| raster | 512 | 1024, splat 8 m | — |

Particles per cell at release, by scenario, for the three detail levels:

| Scenario | columns in disc | Low 32k | Medium 64k | High 128k |
|---|---|---|---|---|
| Breite Ries, r = 100 m | ~200 | 160 | 330 | 650 |
| Dachstein, r = 130 m | ~340 | 100 | 190 | 385 |
| Grossglockner, r = 150 m | ~450 | 70 | 145 | 290 |

Every level is an order of magnitude above 8 per cell; the visual density of the splatted
overlay is what sets the lower limit, not the mechanics.

---

## 4. GPU memory (VRAM) and device limits

### 4.1 Formulas

| Buffer | Size | Old preset | Now: Low / Medium / High |
|---|---|---|---|
| particles | 128 B × N | 16.0 MiB | 4.0 / 8.0 / 16.0 MiB |
| grid accumulators | 20 B × R² × L | 31.3 MiB | 19.5 / 23.4 / 31.3 MiB |
| grid velocity (new) | 16 B × R² × L | — | 15.6 / 18.8 / 25.0 MiB |
| column floor, tile flags | 4 B × R² + 4 B × (R/8)² | 0.4 MiB | 0.4 MiB |
| density raster + RGBA8 texture | 8 B × S² | 8.0 MiB | 8.0 MiB |
| state, readback, timestamps | < 1 KiB | — | — |
| **total** | | **55.6 MiB** | **47.6 / 58.6 / 80.6 MiB** |

(R = grid XY, L = layers, S = raster edge.) The panel shows the live figure; the benchmark
reports it exactly.

The velocity buffer trades 16 B per node for dropping the clear pass and the atomic loads in
G2P; at Medium that is +19 MiB against the old preset's 55.6 MiB total — net about even, because
Medium also halves the particles.

### 4.2 Against real devices

The inputs owned upstream (height, normals, release textures of a 625² region) are a few MiB.
The solver is well under 100 MiB at every preset, so **capacity is not the limit on any
device that runs weBIGeo at all** — 4 GB and 8 GB discrete GPUs and 8 GB unified-memory Macs
(where Metal's working set is capped at roughly two thirds to three quarters of RAM) alike.

What differs between devices is **bandwidth and atomic throughput**, and that is what every
substep is bound by:

| Class | Memory bandwidth (approx.) | Expected vs M5 |
|---|---|---|
| Apple M5 (reference) | ~150 GB/s | 1× |
| Apple M1 / M2 base | ~70 / 100 GB/s | 1.5–2× slower |
| NVIDIA laptop, 4–8 GB (RTX 3050 / 4050 / 4060) | 190–260 GB/s | similar or faster, but shares the frame with the terrain |
| Intel Iris Xe / UHD (shared DDR) | 50–70 GB/s | 2–4× slower, plus weak atomics |

Two capacity caveats remain:

- **The UI ranges go far beyond the presets.** At the top of every slider (2²¹ particles,
  512² × 64 grid, 4096² raster) the solver would want ~1 GiB, and the 20 B × 16.8 M node grid
  buffer (320 MiB) exceeded the 256 MiB storage-binding size the app requires — a validation
  error, which on the web loses the device. The node now clamps particles and grid to the
  device's `maxStorageBufferBindingSize` and logs what it did.
- **VRAM oversubscription is invisible.** On Windows, a 4 GB card whose terrain tile caches,
  browser and other apps fill the dedicated memory gets paged to shared system memory by WDDM
  — everything slows down, gradually, with no error. Task Manager → GPU → "Dedicated GPU memory"
  shows it. This is outside the extension, but it produces exactly the "slower and slower"
  symptom (§5.2).

(Also fixed on the way: the old clear pass dispatched `nodes / 256` workgroups in one dimension,
which at 512² × 64 is 65 536 — one more than WebGPU's 65 535 limit.)

---

## 5. Memory leaks and lifetime bugs

### 5.1 Found in code, all fixed in this branch

| # | What | Where | Rate | Fix |
|---|---|---|---|---|
| L1 | `RawBuffer::read_back_async()` on a non-mappable buffer calls `copy_to_buffer(WGPUDevice…)`, which submits a command buffer it never releases and calls `wgpuDeviceGetQueue()` (a new reference) without releasing it | `webgpu/base/raii/RawBuffer.h` (upstream), triggered by `read_back_state()` | **every run** (4–5 per second while playing) | The solver copies the state into a persistent MapRead buffer *inside its last chunk's command buffer* and maps that; the leaking path is no longer used by the extension |
| L2 | A fresh staging buffer allocated and freed per readback | same | every run | same persistent buffer |
| L3 | `register_pipeline([this]…)`: the registry keeps every factory forever and calls all of them on shader reload (F5). Each graph load (the autostart replaces the default graph at every launch; "Load the MLS-MPM simulation" does it again) leaves a factory with a dangling `this` | `MpmSolverNode` ctor + `RenderResourceRegistry` (upstream, no unregister) | every graph load; **use-after-free on F5** | One factory per registry that rebuilds the pipelines of the solver instances alive at that moment; instances deregister in the destructor |
| L4 | Work-done and map callbacks carried a raw `this`; replacing the graph while a run was in flight would call into a destroyed node | `submit_chunk`, `read_back_state` | on graph replacement mid-run | Callbacks carry a copy of a `shared_ptr<MpmSolverNode*>` that the destructor nulls |
| L5 | Energy-line samples appended once per run, forever; the panel copies all of them into a new vector every frame and refits them | `MpmSolverNode::m_energy_line`, `AvalanchePanel::draw_panel` | +1 per run; O(n) per frame | Capped at 2048 samples (halved when full — path and time stay monotonic, the fit is unaffected) |
| L6 | New bind group every run | `run_impl` | every run | Cached; rebuilt when our buffers, the input textures or the graph run id change |
| L7 | A readback that lands after *Reset* appended a stale pre-reset sample | map callback | per reset | Readbacks are tagged with a reset counter |

L1 and L3 remain in the upstream base code for every other user of those paths (other nodes
still register `[this]` factories). Both are two-to-ten-line fixes there (release the command
buffer and the queue in `copy_to_buffer(WGPUDevice…)`; give `register_pipeline` a handle to
unregister) — worth sending upstream, but outside this extension.

### 5.2 Other causes of "slower the longer it runs"

Ranked by how likely they are to be *visible* within minutes:

1. **Console logging.** `Node::run` / `complete_run` log two `qDebug` lines per run; with the
   overlay node that is four per solver run, ~20 per second while playing. Unless the build
   compiles `qDebug` out, on the web these become `console.log` calls, which Chrome's DevTools
   retains — with DevTools open, the tab grows and slows over minutes. (Not verified against the
   deployed build; the Chrome console of the demo answers it in a second.) (Base code; a release build with `QT_NO_DEBUG_OUTPUT`, or a
   quieter log level for per-run messages, avoids it.)
2. **Thermal throttling.** A sustained 100 % compute load on a fanless MacBook Air or a thin
   laptop drops the GPU clock after a few minutes. Looks exactly like a leak; isn't one.
   The benchmark's ms/run trace over a 10-minute run shows it as a step, not a slope.
3. **VRAM oversubscription** on 4 GB cards (§4.2).
4. L1–L6 above — real, unbounded, but small (hundreds of bytes to a few KB per run; a JS-side
   handle per leaked object on the web).

"Run it again" can also mean *Reset*: resetting reallocates nothing unless a size setting
changed, and did not leak before this branch either (apart from L1/L2, which are per run).

---

## 6. Integration into the main loop, and threading

### 6.1 Is the solver blocking the render thread?

Not on the CPU: there is no synchronous wait anywhere in the solver (the only readbacks are
asynchronous maps of 56-byte and 16-byte buffers). The contention is on the **GPU's single
queue**, which WebGPU exposes as one queue per device. A frame's render pass executes after
the solver chunks that were submitted before it; the chunking bounds that to one chunk per
frame.

### 6.2 Options for running it "in another thread"

| Option | What it buys | Cost / blocker | Verdict |
|---|---|---|---|
| CPU worker thread that encodes and submits | nothing — encoding is µs, the GPU queue is still shared | Dawn device calls from two threads, Qt event-loop integration | no |
| **Budgeted chunks** (implemented): substeps per frame from measured GPU ms per substep | the simulation gets a fixed slice of each frame; fast GPUs get more substeps, slow ones keep their frame rate | none | **yes — done** |
| Second WebGPU device on a worker (native: second Dawn device on a `std::thread`; web: a Worker with its own `GPUDevice`) | GPU work from two queues can overlap or be time-sliced by the driver, so a long chunk no longer delays the frame | Buffers and textures cannot be shared between devices: height/release textures up once per reset (~3 MB), density raster down once per run (4 MB at 1024²) via CPU, or the particle buffer (8 MB at 64k) for a future 3D renderer. Web: Qt-for-WASM threads need SharedArrayBuffer and COOP/COEP headers on the site, and emdawnwebgpu objects are thread-bound | worth a prototype **after** the per-substep cost is down; native first |
| WebGPU multi-queue / async compute | the real answer | not in the WebGPU spec yet | watch |
| Decouple display from simulation (interpolate between the last two run results) | smooth animation at any simulation rate | a second output texture and a blend | cheap, good follow-up |

05-tuning.md's earlier conclusion ("a CPU thread would not help") stands for the CPU; the
second-device route is the only real form of "another thread", and its price is the CPU
round-trip of the results.

---

## 7. Reproducible benchmarks

### 7.1 Kernel benchmark (new): `mpm-mls-doc/bench/`

A standalone WebGPU page that loads the **real WGSL files**, mirrors the node's bindings,
uniform packing and dispatch sequence, and runs them on an **analytic terrain** (a 35° slope
bending into a flat runout, with a gully), so every run is identical work with no network,
no tiles and no UI. It reports per-stage GPU time from timestamp queries, the exact solver
memory, wall time, and a centre-of-mass trace. See [bench/README.md](bench/README.md).

```sh
npx http-server -c-1 .   → http://localhost:8080/mpm-mls-doc/bench/            # your GPU
node mpm-mls-doc/bench/run_headless.mjs --baseline <rev> --out r.json          # A/B
```

Because the P2G accumulates in fixed point (order-independent integer adds), **identical
kernels reproduce the trace bit for bit**; a changed kernel shows up as a trace difference,
which is how the optimisations below were checked for physics changes.

### 7.2 Protocol for showing an improvement

1. Same machine, power adapter plugged in, browser restarted, no other GPU load.
2. Fixed configuration (the defaults = the Breite Ries preset); 3 warm-up runs, 20–40 measured.
3. Report **median ms per run**, the per-stage split, and `simSecondsPerWallSecond`; attach the
   JSON (it records adapter, user agent, commit and whether the shaders were dirty).
4. For a kernel change: `--baseline` against the previous commit, and quote the COM-trace
   deviation next to the speed-up.
5. Repeat on at least one slow device (integrated GPU or base M1) — the complaint came from
   there, and bandwidth-bound and ALU-bound changes rank differently on different GPUs.

### 7.3 Memory and leak protocol (in the app)

- The panel shows the solver's own GPU memory, ms per run, GPU ms per substep and simulated
  seconds per wall second.
- Leak check: play for 10 minutes and sample every 30 s — web: Chrome Task Manager (GPU memory
  and JS memory columns of the tab) with DevTools **closed**; native: RSS (`ps`, Activity
  Monitor) and, on macOS, `MTL_HUD_ENABLED=1` for the Metal memory. A leak is a slope; thermal
  throttling is a step in ms/run with flat memory.
- Graph-reload check (L3/L4): switch *Load the MLS-MPM simulation* 20× and press F5 — before
  this branch that was a use-after-free.

---

## 8. Measurements

All numbers below are from the headless harness on **SwiftShader** (Chromium's CPU Vulkan),
the only "GPU" available where this was written. Absolute times are meaningless for a real GPU;
the *ratios* between stages and between kernel versions are what carry over, with the caveat
that a CPU executes atomics and SVDs with different relative costs than a GPU. The same
commands with `--gpu`, or the page in Chrome, produce the real numbers — please add them to
`bench/results/`.

### 8.1 Old vs new kernels, same configuration (the preset: 131 072 particles, 320² × 16)

`run_headless.mjs --baseline HEAD runs=40` — 3 warm-up + 40 measured runs of 24 substeps,
10.3 s simulated. File: `bench/results/swiftshader-v2-vs-v1-preset.json`.

| Stage, ms per run | v1 (before) | v2 (after) | |
|---|---|---|---|
| clear | 304 | — | pass removed |
| P2G | 4 106 | 3 163 | −23 % (no SVD) |
| grid update | 2 451 | 677 | −72 % (active tiles only) |
| G2P | 3 318 | 3 424 | +3 % (computes τ; plain velocity loads) |
| splat + rasterise | 55 | 58 | |
| **total** | **10 234** | **7 321** | **1.40×** |
| solver memory | 55.6 MiB | 80.6 MiB | + grid velocity buffer |

**Physics:** the centre of mass travelled 107 m; the two traces differ by at most **1.7 mm**
(2 × 10⁻⁵ of the travel). The remaining difference is float rounding — G2P now reads velocities
as floats instead of through the 10⁻⁴ m/s fixed-point round trip, and τ comes from the return
mapping's SVD instead of a fresh one. Same active and plastic particle counts.

The grid share fell from 27 % to 9 % of the run; P2G and G2P are now 90 % of it, which is where
§11's next steps aim. The trace also shows factor 1 of §0 on this slope: after 5.3 s of
simulated time the centre of mass has moved 23 m, after 10.3 s 86 m.

On SwiftShader atomics are comparatively cheap and the SVD comparatively expensive relative to a
GPU, and the grid passes are memory-bound on both; expect the grid saving to carry over in full
and the P2G saving to be smaller on a GPU. The M5 fit of 08 §5 put the old grid passes at ~40 %
of a run (vs 27 % here), so the real-GPU gain from the tiles is likely *larger* than measured
here.

### 8.2 Every material model, 16 384 particles

The return mappings of all three models changed (they now also return τ), so each was
A/B-tested on its own; 2 warm-up + 30 measured runs, 7.7 s simulated. Files:
`bench/results/swiftshader-v2-vs-v1-{dp,ccc}-16k.json` (Stomakhin at this size: §8.4, dt = 0.01).

| Model | v1 ms/run | v2 ms/run | speed-up | COM travel | max trace difference |
|---|---|---|---|---|---|
| Drucker–Prager (Klár 2016, φ = 13.3°) | 3 968 | 1 520 | 2.61× | 64.1 m | 1.4 mm |
| Cohesive Cam Clay (Li 2021 case V) | 3 564 | 1 499 | 2.38× | 64.7 m | 1.4 mm |

With fewer particles the grid passes were 70–75 % of the old run, so the tile change dominates.
In v2 the grid update costs about the same at 16k as at 131k particles (~680 ms per run here):
on SwiftShader that is mostly the fixed cost of launching all 1 600 tile workgroups, each of
which runs a barrier before it can return; on a GPU an idle workgroup costs next to nothing.
§8.3 measures that floor; §11 #4 (indirect dispatch over a compacted tile list) removes it.

### 8.3 The shipped configuration: new kernels at Medium detail

65 536 particles, 320² × 12 (the new graph preset), 40 runs; file `bench/results/swiftshader-v2-medium.json`.

| | old preset, old kernels | Medium, new kernels |
|---|---|---|
| GPU ms per run (24 substeps) | 10 234 | **3 818** — **2.68×** |
| P2G / grid / G2P | 4 106 / 2 755 / 3 318 | 1 514 / 650 / 1 621 |
| solver memory | 55.6 MiB | 58.6 MiB |
| final centre of mass (10.3 s) | 5464.705, 2341.156 | 5464.661, 2341.124 |

Halving the particles moved the centre of mass after 107 m of travel by **4 cm** — the
mechanics at 12.5 m cells do not care; only the rendered cloud gets sparser.

**The grid-update floor.** With an almost empty grid (1 024 particles) the v2 grid update still
costs 650 ms per run at 320² (1 600 tiles) and 187 ms at 160² (400 tiles): on SwiftShader every
tile workgroup pays for its barrier before it can return, whatever the snow does. On a GPU an
idle workgroup is far cheaper, so this is probably an artefact of the CPU backend — but it has
not been measured on one, and indirect dispatch over a list of active tiles (§11 #4) would
remove it everywhere.

### 8.4 Time step: there is headroom

Same slope, 16 384 particles, ~7.5 s simulated at each `dt` (files
`bench/results/swiftshader-dt-*.json`). Distance the centre of mass has moved down the slope,
and mean speed, at t = 7 s:

| Model | dt | c·dt/dx | COM travel at 7 s | mean speed at 7 s | max speed over the run |
|---|---|---|---|---|---|
| Stomakhin (E = 0.14 MPa, c = 18.7 m/s) | 0.01 | 0.015 | 40.19 m | 13.95 m/s | 17.1 m/s |
| | 0.02 | 0.030 | 39.97 m | 13.86 m/s | 17.9 m/s |
| | 0.03 | 0.045 | 39.71 m | 13.81 m/s | 18.6 m/s |
| Cam Clay, Li case V (E = 3 MPa, c = 87 m/s at ρ = 400) | 0.005 | 0.035 | 40.86 m | 15.89 m/s | 24.6 m/s |
| | 0.01 | 0.069 | 40.89 m | 15.90 m/s | 24.6 m/s |
| | 0.02 | 0.139 | 40.81 m | 15.89 m/s | 24.9 m/s |

Tripling `dt` for Stomakhin changes the travel by 1.2 %; Cam Clay at dt = 0.02 — 1.4× the
bound the panel enforces (0.1 · dx / c) — agrees with dt = 0.005 to 0.1 %. Simulated time per
substep is proportional to `dt` and the cost is not, so **the graph preset now uses dt = 0.02**
(2× simulated time per second for the default Stomakhin scenario). The stiff presets are still
pulled under 0.8 × the conservative bound by `apply_material_preset`; relaxing that factor from
0.1 to ~0.2 would double their speed too, but wants a check of the deposition phase (impacts,
piling) on real terrain first, which 7 s on this slope does not reach — §11 #2.


---

## 9. Changes in this branch

### 9.1 Kernels (v2 schedule)

```
before:  clear (all nodes) → P2G (SVD, atomics) → grid update (all nodes) → G2P (atomic loads, SVD)
after:              P2G (atomics, flags tiles) → grid update (flagged tiles, zeroes, writes velocity) → G2P (SVD → τ)
```

- **Active tiles.** Columns are grouped into 8 × 8 tiles. P2G flags the (at most four) tiles
  its 3 × 3-column stencil touches (`mark_tiles`, a load before the store keeps most particles
  from writing). The grid update runs one 8 × 8 workgroup per tile; thread 0 reads the flag,
  `workgroupUniformLoad` distributes it, unflagged tiles return, and thread 0 clears the flag
  for the next substep. Each thread then walks its column's layers.
- **No clear pass.** The grid update is the last reader of the fixed-point accumulators, so it
  zeroes the ones it read (skipping untouched nodes). Nodes outside flagged tiles were never
  written and stay zero; a reset clears the grid once, outside the pass.
- **Velocities as floats.** The grid update writes `grid_velocity` (vec4f per node); G2P reads
  it with plain loads instead of three `atomicLoad`s and the fixed-point round trip.
- **One SVD per substep.** Each model's return mapping now also returns τ = P Fᵀ of the state it
  returns — U diag(2μ(σ² − σ) + λJ(J − 1)) Uᵀ for fixed corotated, U diag(2με + λ tr ε) Uᵀ for
  Hencky — from the U and σ it already has. The particle stores τ's six components in the
  vec3 padding; P2G reads them. The per-particle `volume` slot is gone (it was identical for
  every particle; `settings.particle_volume` is used). As a side effect a density change now
  takes effect immediately, like every other material parameter.

The particle stays 128 B; the uniform layout is unchanged.

### 9.2 Node

L1–L7 of §5, the storage-binding clamps of §4.2, and:

- **GPU timing**: timestamp writes on the solver's own compute pass (two query slots, one per
  chunk in flight, each resolved into its own MapRead buffer; a busy slot is simply not
  timed). Each slot is resolved at a **256 B stride** — `ResolveQuerySet`'s destination
  offset must be a multiple of `QUERY_RESOLVE_BUFFER_ALIGNMENT`, and an unaligned offset
  invalidates the entire command buffer, not just the resolve. Core WebGPU with the `timestamp-query` feature the app already requires, so it works
  on the web. Exposed as `perf_stats().gpu_ms_per_substep`.
- `perf_stats()`: runs since reset, ms per run, solver GPU bytes.

### 9.3 Panel and preset

- **Pacing**: Manual / Smooth (6 ms GPU per frame) / Balanced (14 ms, default) / Fast (30 ms).
  Substeps per frame = budget / measured ms per substep, with hysteresis; `substeps_per_run`
  becomes max(2 × substeps per frame, 24), so a run always spans at least two chunks (the node
  keeps two in flight; a one-chunk run would leave the GPU idle between runs). Both fields are
  read-only in the panel unless Pacing is Manual.
- **Detail**: Low 32k / 10 layers, Medium 64k / 12, High 128k / 16. The graph preset starts at
  Medium, with `dt` = 0.02 s (§8.4).
- Readout: simulated seconds per wall second, substeps per frame, GPU ms per substep,
  ms per run, solver MiB.

### 9.4 How this was verified

- WGSL: every kernel validates with naga (`scripts/validate_wgsl.py` now falls back to naga
  when tint has not been built).
- Kernels: headless A/B against the previous commit on SwiftShader, trace comparison (§8).
- C++: `MpmSolverNode.cpp`, `AvalanchePanel.cpp` and `MpmSolverNodeRenderer.cpp` type-check
  (`g++ -fsyntax-only -Wall -Wextra`, no warnings) against the headers of the Dawn release the
  build pins, Qt 6 and glm — `scripts/syntax_check.sh` sets that up in any container.
  **Not** linked or run in the app: that needs a real build on a machine with a GPU, and is the
  first thing to do with this branch (play the Breite Ries scenario, watch the panel's GPU and
  speed readout, press F5, switch location a few times).

---

## 10. What the literature says about real-time use

*Sources: the papers attached to the Notion "Avalanche Sim" page and its summary sub-page. The
PDFs could not be opened from this environment (their hosts are blocked by its network policy),
so the GPU-implementation points are from the published methods as generally known, not re-read
for this note — check the numbers against the PDFs before quoting them in the report.*

- **Li et al. 2021** (the closest prior work: same method family, CCC, real 3D terrain): 1.9 M
  material points, 0.5 m cells, **8 particles per cell**, Δt = 2 × 10⁻³ s; 100 s of simulated
  time took ≈ 5 h on 36 CPU cores — **~180× slower than real time**. They simulate only the
  release-zone snow, as we do. Our setup trades their 0.5 m cells for 12.5 m (25³ ≈ 15 000×
  fewer cells per volume) and a 5× larger Δt; that trade, not the GPU alone, is what makes
  interactive rates possible — and it is why the slab is sub-cell (05-tuning.md).
- **Operational tools** (com1DFA, Flow-Py) are not real-time either: Flow-Py's regional case
  took 3 h 45 min and com1DFA is 1–2 orders of magnitude slower. Real-time is the contribution,
  so a clean benchmark (§7) is worth having in the report.
- **MLS-MPM (Hu et al. 2018)** — the formulation we use — folds the stress and APIC terms into
  one affine matrix, so P2G needs no weight gradients. That is already exploited; the stress
  itself still needs the SVD, which is why computing it once (§9.1) matters.
- **GPU MPM practice** (Gao et al. 2018, *GPU optimization of material point methods*; Wang et
  al. 2020, *A massively parallel and scalable multi-GPU material point method* — linked from the
  Notion page): particles **sorted/binned by cell**, P2G **reduced within a warp before the
  atomic** (neighbouring lanes that hit the same node add in registers first), sparse grid
  blocks allocated only where particles are, and G2P and the next P2G **fused** into one kernel
  so particle data is read once per substep. The active-tile scheme of §9.1 is the WebGPU-sized
  version of the sparse-block idea; the other three are the next steps (§11).
- **CK-MPM (Liu et al. 2025)**: a compact kernel that keeps C² smoothness while a particle
  touches only the nodes of its own cell (on two staggered grids). For us the point is fewer
  node accesses — and therefore fewer atomics — per particle than the 27 of the quadratic
  B-spline, plus less numerical diffusion. It touches only the transfer kernels (05-tuning.md).
- **PB-MPM (Lewin 2024)**: unconditionally stable at frame-sized time steps with an open-source
  WebGPU implementation — the real-time end of the spectrum, at the cost of the continuum
  mechanics the snow models need. Parked, as the Notion page concludes; noted here because its
  stability at large Δt is exactly what our explicit scheme lacks.

---

## 11. Further improvements, ranked

| # | Idea | Expected effect | Effort | Notes |
|---|---|---|---|---|
| 1 | **Subgroup (warp) pre-reduction in P2G** + keep particles roughly cell-sorted | P2G atomics down by the lanes-per-node factor in dense regions; this is the main remaining cost | M | WGSL `subgroups` feature (Chrome 134+, Dawn); needs a fallback path. Sorting: a counting sort by tile every N runs, or seed particles in Morton order (they stay near their neighbours in a flow) |
| 2 | **Auto dt from CFL** with a relaxed safety factor (0.1 → ~0.2 · dx / c), capped at ~0.03 s | ~2× simulated time per substep for the stiff Li / Cam-Clay presets (Stomakhin already went to 0.02) | S | §8.4 shows it on the slope; check deposition on real terrain first |
| 3 | **Fuse G2P and the next P2G** (G2P2G, Wang et al. 2020) | one particle read/write per substep instead of two; one dispatch less | M | Needs double-buffered grid (read velocities of step n while accumulating step n+1) |
| 4 | **Indirect dispatch over a compacted tile list** | grid update cost ∝ active tiles without launching the idle workgroups | S–M | Worth it only for domains ≫ 320² |
| 5 | **Leaner particle**: f16 for C (and τ), drop `mass` (flag bit) | particle bandwidth −30–40 % | M | Watch the APIC precision; F must stay f32 |
| 6 | **CK-MPM kernel** | fewer node accesses per particle, less diffusion | L | stretch goal already |
| 7 | **Display interpolation** between the last two runs | smooth motion at any simulation speed | S | pairs well with low sim rates on weak GPUs |
| 8 | Second device / worker (§6.2) | frame and simulation overlap | L | after 1–3 |
| 9 | Quieter per-run logging, `copy_to_buffer` release fix, registry unregister | removes the upstream parts of §5 | S | upstream PRs |
