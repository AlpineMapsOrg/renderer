# Benchmark results

*Generated from `bench/results/*.json` by `bench/results_to_markdown.py` — do not edit by hand.*
*What the numbers mean, and the scenarios behind them: [09-performance-analysis.md](09-performance-analysis.md) §3 and §7. How to run and add results: [bench/README.md](bench/README.md).*

Each run is `substepsPerRun` = 24 MPM substeps of the real WGSL kernels on an analytic 35° slope (4 km domain, 12.5 m cells), median over the measured runs. **GPU ms** are timestamp-query times summed over the stages; the *ceiling* is simulated seconds per wall second with the GPU doing nothing else, i.e. an upper bound for the app.

## Real GPU

Device: **Apple M5** (WebGPU adapter `Apple M5, apple metal-3`); Chrome 152 (2026-09-28), Chrome 152 (2026-09-29), Chrome 158 (2026-11-07).

| file | material | particles | grid | dt | GPU ms / run | GPU ms / substep | P2G / grid / G2P ms per run | GPU-bound ceiling | solver MiB |
|---|---|---|---|---|---|---|---|---|---|
| `m5-chrome152-high-ccc.json` | Cam Clay | 131,072 | 320² × 16 | 0.01 | 28.24 | **1.177** | 18.52 / 1.82 / 7.62 (66 % P2G) | 8.03× | 80.6 |
| `m5-chrome152-high.json` | Stomakhin | 131,072 | 320² × 16 | 0.01 | 27.68 | **1.153** | 18.05 / 1.43 / 7.82 (65 % P2G) | 8.33× | 80.6 |
| `m5-chrome152-medium-ccc.json` | Cam Clay | 65,536 | 320² × 12 | 0.01 | 15.28 | **0.637** | 10.49 / 1.06 / 3.50 (69 % P2G) | 15.6× | 58.6 |
| `m5-chrome152-medium-dp.json` | Drucker–Prager | 65,536 | 320² × 12 | 0.01 | 15.93 | **0.664** | 10.71 / 1.28 / 3.71 (67 % P2G) | 15.2× | 58.6 |
| `m5-chrome152-medium-pass.json` | Stomakhin | 65,536 | 320² × 12 | 0.01 | 13.92 | **0.580** | — (per-chunk timing) | 16.1× | 58.6 |
| `m5-chrome152-medium.json` | Stomakhin | 65,536 | 320² × 12 | 0.01 | 14.02 | **0.584** | 10.34 / 0.51 / 3.05 (74 % P2G) | 15.5× | 58.6 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-131k.json` (v2+sort kernels, bac8fd99) | Cam Clay | 131,072 | 320² × 16 | 0.01 | 12.49 | **0.521** | 5.59 / 0.51 / 5.96 (45 % P2G) | 18.2× | 83.5 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-131k.json` (baseline, v2 kernels, bac8fd99) | Cam Clay | 131,072 | 320² × 16 | 0.01 | 25.51 | **1.063** | 17.94 / 0.55 / 6.84 (70 % P2G) | 9.12× | 83.0 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-1m.json` (v2+sort kernels, bac8fd99) | Cam Clay | 1,048,576 | 320² × 16 | 0.01 | 106.07 | **4.420** | 41.44 / 0.46 / 62.04 (39 % P2G) | 2.24× | 199.0 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-1m.json` (baseline, v2 kernels, bac8fd99) | Cam Clay | 1,048,576 | 320² × 16 | 0.01 | 187.96 | **7.832** | 124.67 / 0.44 / 62.04 (66 % P2G) | 1.27× | 195.0 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-65k.json` (v2+sort kernels, bac8fd99) | Cam Clay | 65,536 | 320² × 12 | 0.01 | 6.42 | **0.268** | 2.87 / 0.33 / 3.01 (45 % P2G) | 33.8× | 61.2 |
| `m5-chrome158-sorted-p2g-vs-main-ccc-65k.json` (baseline, v2 kernels, bac8fd99) | Cam Clay | 65,536 | 320² × 12 | 0.01 | 12.08 | **0.504** | 8.56 / 0.36 / 3.06 (71 % P2G) | 18.6× | 60.9 |
| `m5-chrome158-sorted-p2g-vs-main-dp-131k.json` (v2+sort kernels, bac8fd99) | Drucker–Prager | 131,072 | 320² × 16 | 0.01 | 12.06 | **0.502** | 5.40 / 0.47 / 5.79 (45 % P2G) | 18.8× | 83.5 |
| `m5-chrome158-sorted-p2g-vs-main-dp-131k.json` (baseline, v2 kernels, bac8fd99) | Drucker–Prager | 131,072 | 320² × 16 | 0.01 | 25.39 | **1.058** | 18.44 / 0.47 / 6.31 (73 % P2G) | 9.16× | 83.0 |
| `m5-chrome158-sorted-p2g-vs-main-stomakhin-131k.json` (v2+sort kernels, bac8fd99) | Stomakhin | 131,072 | 320² × 16 | 0.01 | 12.06 | **0.503** | 5.37 / 0.48 / 5.81 (45 % P2G) | 18.8× | 83.5 |
| `m5-chrome158-sorted-p2g-vs-main-stomakhin-131k.json` (baseline, v2 kernels, bac8fd99) | Stomakhin | 131,072 | 320² × 16 | 0.01 | 23.18 | **0.966** | 16.20 / 0.45 / 6.35 (70 % P2G) | 10× | 83.0 |

Reading it: with P2G in buffer order (`v2` kernels), its fixed-point atomics are ~70 % of the GPU time whatever the material. With the particles sorted by tile and summed in workgroup memory (`v2+sort`, the `sorted-p2g` files), P2G is about 3× faster and ~45 % of the time; G2P is now the largest stage. Cam Clay (the benchmark default) costs ~9 % more per substep than Stomakhin. `medium-pass` times one query pair per chunk as the app does; it agrees with the per-stage total.

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
| `m5-chrome158-sorted-p2g-vs-main-ccc-131k.json` | **2.042×** | 34.0 m | 0.0 mm |
| `m5-chrome158-sorted-p2g-vs-main-ccc-1m.json` | **1.772×** | 34.0 m | 0.0 mm |
| `m5-chrome158-sorted-p2g-vs-main-ccc-65k.json` | **1.882×** | 34.0 m | 0.0 mm |
| `m5-chrome158-sorted-p2g-vs-main-dp-131k.json` | **2.105×** | 33.5 m | 0.0 mm |
| `m5-chrome158-sorted-p2g-vs-main-stomakhin-131k.json` | **1.922×** | 33.1 m | 0.0 mm |
| `swiftshader-v2-vs-v1-ccc-16k.json` | **2.377×** | 64.7 m | 1.4 mm |
| `swiftshader-v2-vs-v1-dp-16k.json` | **2.611×** | 64.1 m | 1.4 mm |
| `swiftshader-v2-vs-v1-preset.json` | **1.398×** | 107.3 m | 1.7 mm |
