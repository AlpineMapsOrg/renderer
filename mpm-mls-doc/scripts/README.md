# Python scripts

Every Python script in `mpm-mls-doc/`: what it checks or produces, what it needs, how to run
it. The scripts are the evidence behind the docs. [06-verification.md](../06-verification.md)
has their results; [avaframe/README.md](../avaframe/README.md) and
[bench/README.md](../bench/README.md) have the details of those two workflows.

Times are measured on an M5; `—` where none was measured.

| Script | What it does | Needs | Time |
|---|---|---|---|
| **Shaders** | | | |
| [`validate_wgsl.py`](validate_wgsl.py) | Resolves the `///use` includes and validates every MPM kernel and the avalanche render shaders with tint | built `extern/dawn` (or `naga`) | seconds |
| [`check_refactor_preserving.py`](check_refactor_preserving.py) | Shows a shader refactor changes no behaviour: resolved WGSL of the working tree against git `HEAD`, per function | git | seconds |
| **Python ports of the shaders** | | | |
| [`test_svd.py`](test_svd.py) | `svd3()` of `mpm_common.wgsl` against numpy, 700+ matrices | numpy | seconds |
| [`test_material_dp.py`](test_material_dp.py) | Drucker–Prager return mapping (Klár 2016) against its yield surface | numpy | seconds |
| [`test_material_ccc.py`](test_material_ccc.py) | Cam Clay return mapping, hardening and Wolper's §6.2.3 against the yield surface; Li 2021 Table 1 | numpy | seconds |
| [`test_friction.py`](test_friction.py) | Coulomb and Voellmy basal friction on a slope against closed-form mechanics (terminal velocity, sticking) | numpy | seconds |
| [`test_mpm.py`](test_mpm.py) | The P2G / grid / G2P loop on a flat floor: mass, gravity, landing, settling | numpy | ~1 min |
| [`test_energy_line.py`](test_energy_line.py) | A sliding slab against the energy line (Tonnel 2023 §5.2): the coupled loop removes μ of energy height per metre | numpy | ~3 min |
| [`test_sheet_fixed_point.py`](test_sheet_fixed_point.py) | A thin slab at device scale with the shader's fixed-point accumulation emulated (the energy-pump bug) | numpy | ~4 min |
| [`make_golden.py`](make_golden.py) | Runs the ports on fixed inputs and writes the golden file for the GPU tests | numpy | seconds |
| [`make_golden_substep.py`](make_golden_substep.py) | Advances 300 particles by 1 and 3 substeps in float64, mirroring the kernels, for the whole-kernel test | numpy | seconds |
| **AvaFrame comparison** ([avaframe/README.md](../avaframe/README.md)) | | | |
| [`prepare_dataset.py`](../avaframe/prepare_dataset.py) | AvaFrame test cases → `extern/avaframe_sandbox/<dataset>/`: DEM, release and entrainment rasters, com1DFA references, AIMEC path | `avaframe/.venv` | — |
| [`run_com1dfa.py`](../avaframe/run_com1dfa.py) | com1DFA 2.1 on a dataset: `null`, `coulomb`, `erosion`, `plowing` | `.venv-avaframe` | — |
| [`run_com4flowpy.py`](../avaframe/run_com4flowpy.py) | com4FlowPy on a dataset, stored like an MPM run | `.venv-avaframe` | 5–50 min per dataset |
| [`compare_aimec.py`](../avaframe/compare_aimec.py) | Rates a run against a com1DFA reference with AvaFrame's AIMEC | `.venv-avaframe` | — |
| [`summarise.py`](../avaframe/summarise.py) | AIMEC on every labelled run, then the tables of 11-avaframe-comparison.md | `.venv-avaframe` | ~15 min |
| [`report.py`](../avaframe/report.py) | The HTML report with images of every run | `.venv-avaframe` | — |
| [`tools/map_release_figure.py`](../avaframe/tools/map_release_figure.py) | The figure of 14-avaframe-release-on-map.md | `avaframe/.venv` | seconds |
| **Benchmark** ([bench/README.md](../bench/README.md)) | | | |
| [`results_to_markdown.py`](../bench/results_to_markdown.py) | `bench/results/*.json` → 10-benchmark-results.md | Python 3 only | seconds |

Not Python, but here as well: [`syntax_check.sh`](syntax_check.sh) type-checks the MPM C++
files with `g++ -fsyntax-only`, without Qt, Dawn or a GPU.

## Setup

Three virtual environments, all ignored by git:

```bash
# scripts/: numpy only
python3 -m venv mpm-mls-doc/scripts/.venv
mpm-mls-doc/scripts/.venv/bin/pip install numpy

# avaframe/: dataset preparation and figures (numpy, pyproj)
python3 -m venv mpm-mls-doc/avaframe/.venv
mpm-mls-doc/avaframe/.venv/bin/pip install -r mpm-mls-doc/avaframe/requirements.txt

# avaframe/: AvaFrame 2.1 itself, which needs Python < 3.14 (the system Python 3.9 works)
/usr/bin/python3 -m venv mpm-mls-doc/avaframe/.venv-avaframe
mpm-mls-doc/avaframe/.venv-avaframe/bin/pip install avaframe==2.1
```

`validate_wgsl.py`, `check_refactor_preserving.py` and `results_to_markdown.py` need no venv.

## Shaders

```bash
python3 mpm-mls-doc/scripts/validate_wgsl.py
```

Run it after every shader change: WGSL otherwise only fails at runtime, when the app builds
the pipeline. It uses the tint in `extern/dawn/install/Release/bin/` (there once the app has
been built) and falls back to `naga` (`cargo install naga-cli`). A kernel that fails leaves
its flattened source as `_resolved_<kernel>.wgsl` next to the script.

```bash
python3 mpm-mls-doc/scripts/check_refactor_preserving.py
```

Use it for a refactor that should not change behaviour. Its `RENAMED` / `DISPATCHERS` tables
hold the intended renames of the refactor it was written for (the material and friction
dispatchers); edit them for a new refactor.

## Python ports of the shaders

Each `test_*.py` ports a shader function or the whole loop line by line, in float64, and checks
it against something independent: numpy, a yield surface, closed-form mechanics, or a paper's
test. A bug in a port is a bug in the shader, as long as the two stay in sync. Run from
`scripts/`, since the ports import each other:

```bash
cd mpm-mls-doc/scripts
./.venv/bin/python test_svd.py
./.venv/bin/python test_material_dp.py
./.venv/bin/python test_material_ccc.py
./.venv/bin/python test_friction.py
./.venv/bin/python test_mpm.py                              # Stomakhin
MPM_MODEL=drucker_prager ./.venv/bin/python test_mpm.py
MPM_MODEL=ccc ./.venv/bin/python test_mpm.py
./.venv/bin/python test_energy_line.py
./.venv/bin/python test_sheet_fixed_point.py                # QUICK=1 skips the old scheme
```

Each prints its checks and `ALL OK`, or a `FAIL` line per failing check. `test_mpm.py` ports an
older version of the loop: a dense grid instead of the terrain-following band, and the stress
computed in P2G instead of being stored by G2P. Its maths is the same.

### GPU golden tests

The ports check the formulas; the GPU golden tests check that the WGSL does the same.
`make_golden.py` runs the ports on fixed inputs and writes them with the ports' outputs to
`unittests/webgpu_compute/golden/mpm_golden.json`. The C++ unit test
`unittests/webgpu_compute/test_MpmGolden.cpp` runs the real shader functions on the GPU on
the same inputs and compares. Rerun `make_golden.py` after changing a port, and commit the
JSON with it.

```bash
cd mpm-mls-doc/scripts && ./.venv/bin/python make_golden.py && cd ../..
cmake -DALP_BUILD_UNITTESTS=ON build/webgpu_app_macos_release
cmake --build build/webgpu_app_macos_release --target unittests_webgpu_compute
./build/webgpu_app_macos_release/unittests/webgpu_compute/unittests_webgpu_compute
```

### Whole-kernel substep test

The golden tests above check functions. `make_golden_substep.py` writes the reference for
`bench/check_substep.mjs`, which runs the real kernels (particle sort, P2G, grid update, G2P)
on the GPU from a known particle state (a free scenario of three substeps and a contact
scenario of one) and compares every particle field with the float64 reference. It needs Playwright (`npm i -g playwright`); `--chrome` uses the installed Chrome, which
is the GPU on macOS, and without it SwiftShader (3 s, no GPU needed).

```bash
cd mpm-mls-doc/scripts && ./.venv/bin/python make_golden_substep.py && cd ../..
node mpm-mls-doc/bench/check_substep.mjs --chrome
node mpm-mls-doc/bench/check_substep.mjs --chrome --shaders-rev <git-rev>   # the shaders of another revision
```

Results and tolerances: [06-verification.md §4p](../06-verification.md).

The cases of the first golden test keep a margin to every branch decision of the return mappings, because float32 may
legitimately take the other branch right at one. The tests need a GPU and do not run on CI.
Results and tolerances: [06-verification.md §4o](../06-verification.md).

## AvaFrame comparison

The scripts in [`avaframe/`](../avaframe/) prepare AvaFrame's five real avalanches, compute
the com1DFA and com4FlowPy references, and rate the app's runs with AIMEC. Run them from the
repository root, in this order:

```bash
mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py --real
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/run_com1dfa.py         # optional: com1DFA 2.1 references
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/run_com4flowpy.py avaKot
# ... the app's batch runs (avaframe/README.md, "Batch mode") ...
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/compare_aimec.py --latest avaKot
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/summarise.py --jobs 4
mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/report.py
```

`prepare_dataset.py --real` rewrites `index.json` and removes the `mpm/` and `com4FlowPy/`
run folders of the datasets it prepares; back up `extern/avaframe_sandbox/` first if it holds
runs. `compare_aimec.py --reference null|erosion|plowing|coulomb|auto` picks the com1DFA 2.1
run of `run_com1dfa.py` instead of the benchmark. The default is AvaFrame's null-test
benchmark.

## Benchmark

```bash
python3 mpm-mls-doc/bench/results_to_markdown.py
```

It regenerates [10-benchmark-results.md](../10-benchmark-results.md) from the checked-in
`bench/results/*.json`, so the tables cannot drift from the results. The benchmark itself
runs in the browser ([bench/README.md](../bench/README.md)).
