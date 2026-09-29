# Benchmark results

*Generated from `bench/results/*.json` by `bench/results_to_markdown.py` — do not edit by hand.*
*What the numbers mean, and the scenarios behind them: [09-performance-analysis.md](09-performance-analysis.md) §7–8. How to run and add results: [bench/README.md](bench/README.md).*

Each run is `substepsPerRun` = 24 MPM substeps of the real WGSL kernels on an analytic 35° slope (4 km domain, 12.5 m cells), median over the measured runs. **GPU ms** are timestamp-query times summed over the stages; the *ceiling* is simulated seconds per wall second with the GPU doing nothing else, i.e. an upper bound for the app.

## Real GPU

Device: **Apple M5** (WebGPU adapter `apple metal-3`), Chrome 152, 2026-09-28/29.

| file | material | particles | grid | dt | GPU ms / run | GPU ms / substep | P2G / grid / G2P ms per run | GPU-bound ceiling | solver MiB |
|---|---|---|---|---|---|---|---|---|---|
| `m5-chrome152-high-ccc.json` | Cam Clay | 131,072 | 320² × 16 | 0.01 | 28.24 | **1.177** | 18.52 / 1.82 / 7.62 (66 % P2G) | 8.03× | 80.6 |
| `m5-chrome152-high.json` | Stomakhin | 131,072 | 320² × 16 | 0.01 | 27.68 | **1.153** | 18.05 / 1.43 / 7.82 (65 % P2G) | 8.33× | 80.6 |
| `m5-chrome152-medium-ccc.json` | Cam Clay | 65,536 | 320² × 12 | 0.01 | 15.28 | **0.637** | 10.49 / 1.06 / 3.50 (69 % P2G) | 15.6× | 58.6 |
| `m5-chrome152-medium-dp.json` | Drucker–Prager | 65,536 | 320² × 12 | 0.01 | 15.93 | **0.664** | 10.71 / 1.28 / 3.71 (67 % P2G) | 15.2× | 58.6 |
| `m5-chrome152-medium-pass.json` | Stomakhin | 65,536 | 320² × 12 | 0.01 | 13.92 | **0.580** | — (per-chunk timing) | 16.1× | 58.6 |
| `m5-chrome152-medium.json` | Stomakhin | 65,536 | 320² × 12 | 0.01 | 14.02 | **0.584** | 10.34 / 0.51 / 3.05 (74 % P2G) | 15.5× | 58.6 |

Reading it: P2G's fixed-point atomics are ~70 % of the GPU time whatever the material; Cam Clay (the benchmark default) costs ~9 % more per substep than Stomakhin. `medium-pass` times one query pair per chunk as the app does; it agrees with the per-stage total.

## SwiftShader (CPU Vulkan) — ratios only

Absolute times say nothing about a GPU; these files exist for the old-vs-new comparison and the physics check (`comparison.physics`: how far the centre-of-mass trace moved between the kernels).

| file | material | particles | grid | dt | GPU ms / run | GPU ms / substep | P2G / grid / G2P ms per run | GPU-bound ceiling | solver MiB |
|---|---|---|---|---|---|---|---|---|---|
| `swiftshader-baseline-v1.json` (v1 kernels, 4d2f534) | Stomakhin | 131,072 | 320² × 16 | 0.01 | 9909.67 | **412.903** | 3998.12 / 2348.05 / 3214.27 (40 % P2G) | 0.024× | 55.6 |
| `swiftshader-dt-ccc-0.005.json` (v2 kernels, b20c44b) | Cam Clay | 16,384 | 320² × 16 | 0.005 | 1486.03 | **61.918** | 386.83 / 665.39 / 416.44 (26 % P2G) | 0.081× | 66.6 |
| `swiftshader-dt-ccc-0.01.json` (v2 kernels, b20c44b) | Cam Clay | 16,384 | 320² × 16 | 0.01 | 1485.51 | **61.896** | 382.69 / 674.42 / 410.60 (26 % P2G) | 0.163× | 66.6 |
| `swiftshader-dt-ccc-0.02.json` (v2 kernels, b20c44b) | Cam Clay | 16,384 | 320² × 16 | 0.02 | 1494.71 | **62.280** | 386.75 / 670.92 / 419.03 (26 % P2G) | 0.325× | 66.6 |
| `swiftshader-dt-stomakhin-0.01.json` (v2 kernels, b20c44b) | Stomakhin | 16,384 | 320² × 16 | 0.01 | 1479.81 | **61.659** | 382.00 / 667.12 / 413.01 (26 % P2G) | 0.162× | 66.6 |
| `swiftshader-dt-stomakhin-0.02.json` (v2 kernels, b20c44b) | Stomakhin | 16,384 | 320² × 16 | 0.02 | 1483.16 | **61.798** | 386.75 / 664.15 / 414.57 (26 % P2G) | 0.328× | 66.6 |
| `swiftshader-dt-stomakhin-0.03.json` (v2 kernels, b20c44b) | Stomakhin | 16,384 | 320² × 16 | 0.03 | 1472.53 | **61.356** | 380.01 / 661.57 / 412.70 (26 % P2G) | 0.487× | 66.6 |
| `swiftshader-v2-medium.json` (v2 kernels, b20c44b) | Stomakhin | 65,536 | 320² × 12 | 0.01 | 3817.86 | **159.077** | 1513.76 / 649.63 / 1621.31 (40 % P2G) | 0.063× | 58.6 |
| `swiftshader-v2-vs-v1-ccc-16k.json` (v2 kernels, b20c44b) | Cam Clay | 16,384 | 320² × 16 | 0.01 | 1499.11 | **62.463** | 391.39 / 667.65 / 421.83 (26 % P2G) | 0.16× | 66.6 |
| `swiftshader-v2-vs-v1-ccc-16k.json` (baseline, v1 kernels, b20c44b) | Cam Clay | 16,384 | 320² × 16 | 0.01 | 3563.63 | **148.485** | 501.64 / 2339.47 / 410.29 (14 % P2G) | 0.068× | 41.6 |
| `swiftshader-v2-vs-v1-dp-16k.json` (v2 kernels, b20c44b) | Drucker–Prager | 16,384 | 320² × 16 | 0.01 | 1519.95 | **63.331** | 389.91 / 687.23 / 424.68 (26 % P2G) | 0.158× | 66.6 |
| `swiftshader-v2-vs-v1-dp-16k.json` (baseline, v1 kernels, b20c44b) | Drucker–Prager | 16,384 | 320² × 16 | 0.01 | 3968.27 | **165.345** | 551.83 / 2610.96 / 458.05 (14 % P2G) | 0.065× | 41.6 |
| `swiftshader-v2-vs-v1-preset.json` (v2 kernels, 4d2f534) | Stomakhin | 131,072 | 320² × 16 | 0.01 | 7321.37 | **305.057** | 3163.32 / 676.61 / 3423.83 (43 % P2G) | 0.034× | 80.6 |
| `swiftshader-v2-vs-v1-preset.json` (baseline, v1 kernels, 4d2f534) | Stomakhin | 131,072 | 320² × 16 | 0.01 | 10233.83 | **426.410** | 4105.74 / 2451.22 / 3317.63 (40 % P2G) | 0.024× | 55.6 |

### Old vs new kernels

| file | GPU speed-up | centre-of-mass travel | max trace difference |
|---|---|---|---|
| `swiftshader-v2-vs-v1-ccc-16k.json` | **2.377×** | 64.7 m | 1.4 mm |
| `swiftshader-v2-vs-v1-dp-16k.json` | **2.611×** | 64.1 m | 1.4 mm |
| `swiftshader-v2-vs-v1-preset.json` | **1.398×** | 107.3 m | 1.7 mm |
