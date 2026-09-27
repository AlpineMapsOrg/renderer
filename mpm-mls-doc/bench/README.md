# MLS-MPM kernel benchmark

A standalone WebGPU page that runs the solver's **real WGSL kernels** (loaded straight from
`webgpu/compute/shaders/`) with the same bindings, uniform packing and dispatch sequence as
`MpmSolverNode`, on a synthetic, analytic slope. No Qt, no tiles, no network: a given
configuration is exactly the same work on every run, which is what makes numbers comparable
across commits and machines. Background and methodology: [../09-performance-analysis.md](../09-performance-analysis.md).

## In a browser, on your GPU (the real numbers)

```sh
# from the repository root
npx http-server -c-1 .          # or: python3 -m http.server
# open http://localhost:8080/mpm-mls-doc/bench/
```

Press *Run benchmark*, then *Save JSON*. Every setting is also a query parameter:
`?particles=65536&gridLayers=12&runs=30&autorun=1`. Per-stage GPU times come from timestamp
queries; Chrome quantises them to 0.1 ms unless
`chrome://flags/#enable-webgpu-developer-features` is on (the totals are fine either way).

## Headless, and A/B against a git revision

```sh
npm i -g playwright            # once
node mpm-mls-doc/bench/run_headless.mjs                              # SwiftShader (CPU)
node mpm-mls-doc/bench/run_headless.mjs --gpu                        # the machine's GPU
node mpm-mls-doc/bench/run_headless.mjs --baseline HEAD~3 --out r.json particles=65536
```

`--baseline <rev>` exports that revision's shaders with `git show` and runs the same
configuration on both, then prints the GPU speed-up and how far the centre-of-mass trace
moved apart. The kernel schedule (v1: separate clear pass; v2: tile-flagged grid update, no
clear) is detected from the shader sources, so old revisions still run.

SwiftShader runs anywhere (CI, containers) but its absolute timings say nothing about a GPU;
use it for relative numbers and for the physics check.

## What is measured

| Field | Meaning |
|---|---|
| `perRun.stagesMs` | GPU ms per stage, summed over the run's substeps (timestamp queries, one compute pass per stage) |
| `perRun.gpuMs` | Sum of the above = GPU cost of one run |
| `perSubstepGpuMs` | `gpuMs / substepsPerRun` - the number the app's pacing uses |
| `perRun.wallMsMedian` | Submit to `onSubmittedWorkDone`, median over the measured runs |
| `simSecondsPerWallSecond` | Simulated time per wall second with the GPU doing nothing else - the ceiling for the app |
| `memory.solverBytes` | GPU memory the solver owns (buffers + output texture), exact |
| `physics.trace` | Centre of mass and speeds after every run - the fingerprint for A/B |

`timing=pass` records one timestamp pair per submitted chunk instead of per stage, which is
what the app does and avoids the per-pass overhead (use it for the total, `stages` for the
breakdown). The fixed-point P2G is order-independent, so identical kernels reproduce the
trace bit for bit on the same device.

## Results in this directory

`results/*.json` are runs checked in as reference points; the file name says device and
configuration. Add your own (M-series Mac, a 4 GB NVIDIA laptop, ...) the same way.
