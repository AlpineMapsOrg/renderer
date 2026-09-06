# Verification

What was actually checked, how, and what wasn't. Keep the distinction — it matters for the
report, where "verified" and "compiles" are very different claims.

## 1. Shader validation — `tint`

Dawn ships a WGSL validator, already vendored:

```
extern/dawn/install/Release/bin/tint
```

The kernels use weBIGeo's `///use` include directive, which `tint` doesn't understand, so
the includes have to be resolved first. Script is in the session scratchpad
(`validate_wgsl.py`): it inlines `///use name` from `webgpu/compute/shaders/` and
`///use webgpu::name` from `webgpu/base/shaders/`, strips other `///` directives, then runs
`tint --format wgsl` on each kernel.

All 8 kernels pass. Worth re-running after any shader edit — much faster than launching the
app, and WGSL otherwise only fails at runtime.

## 2. SVD — checked against numpy

The 3×3 SVD is the highest-risk piece: a bug there produces plausible-looking garbage rather
than an obvious failure. Ported **verbatim** to Python (same scalar formulas, same rotation
order) and tested against numpy:

| Case | Result |
|---|---|
| Identity, pure rotation, uniform compression, anisotropic | exact reconstruction |
| **Inverted** (det F < 0) | correct — signed σ₃ reproduces it |
| Near-degenerate (σ₃ ≈ 1e-7) | stable, orthonormal U/V |
| 200 near-identity + 500 fully random matrices | max reconstruction error < 1e-4 |

All cases: `U Σ Vᵀ = F`, U and V orthonormal with det = +1.

## 3. MPM loop — offline port

The plan's Phase 2 ("de-risk the algorithm outside WGSL"), done after the fact rather than
before. P2G / grid update / G2P ported to Python with the same normalised-mass convention
and a flat floor instead of a DEM.

Results over 900 substeps, 150 particles:

| Check | Result |
|---|---|
| Mass conservation | **exact** (150.00 throughout) |
| Free fall | v = 5.886 m/s after 0.6 s — matches `g·t` to 3 decimals |
| Impact and settling | 5.98 → 1.48 → 0.19 → 0.06 m/s, comes to rest |
| Penetration | min z = 3.00 exactly — rests on the floor, no sinking |
| Deposit shape | mean z stabilises at 4.00 — a pile, not a collapsed plane |
| **Plastic compaction** | mean `jp` 1.0 → 0.746 on impact |

That last row is the important one: the snow **permanently densifies on impact instead of
bouncing back elastically**. That is the Stomakhin model doing its job, and it is the
qualitative behaviour the proposal was after.

The free-fall result is a strong signal for the transfer chain specifically — reproducing
`g·t` exactly means mass-weighted P2G and G2P round-trip without loss.

## 4. Runtime — real device

Booted with the MPM graph as the startup preset and confirmed (via a temporary `qInfo`) that
all 8 shaders compiled and all 8 compute pipelines were created on Metal with **no Dawn
validation errors**. Temporary changes reverted.

Note `register_shader` / `register_bind_group_layout` / `register_pipeline` build
**immediately** when a device already exists, so constructing the node is what compiles the
shaders — loading the graph is a real test, not just a parse.

## 5. On real terrain

Runs end to end on the Schneeberg DEM: seeds, flows downhill, deposits, animates, and the
domain-centre readout matches the intended coordinates.

**Not quantitatively validated.** No comparison against a real avalanche, a reference
implementation, or measured runout. The proposal is explicit that this is educational, and
that should stay explicit in the report.

## Bugs found and fixed along the way

Worth keeping — they are the interesting part of the implementation story.

1. **SIGSEGV on any transport control before a full graph run.** `is_socket_connected()`
   returning true doesn't mean the payload exists — an output socket returning
   `unique_ptr::get()` is null until its node has run. `rerun()` re-runs a single node with
   the last buffered context, so the UI could reach `run_impl()` with a null height texture.
   Fixed with `has_valid_inputs()` (checks connectivity *and* non-null) plus disabled UI.

2. **Invisible output.** Single-texel splatting at 1 m/texel put 65 k particles into ~6% of
   a million texels at count 1. Rendering correctly, invisible in practice. Fixed by
   splatting discs and deriving the density reference instead of hard-coding it.

3. **Animation stopping.** Stepping was driven from the node editor's settings panel, which
   only renders while that editor is open *and* the node selected. Moved to
   `AvalanchePanel::draw()`, which runs every frame regardless of panel visibility.

4. **Normalized coordinates drifting.** Domain/release positions as region fractions moved
   relative to the ground whenever tile snapping changed the region. Switched to lat/lon,
   converted where the region bounds are known.

5. **`qDebug()` is filtered** in this app's logger — use `qInfo()` for anything that needs to
   show up in the console. Cost an entire debugging round to notice.

## Re-running the checks

Scripts are in [`scripts/`](scripts/) — they are the evidence behind every claim on this
page, so they live with the docs rather than in a scratchpad.

```bash
cd mpm-mls-doc/scripts

# 1. shader validation (no dependencies, needs a built extern/dawn)
python3 validate_wgsl.py

# 2 + 3. offline maths (needs numpy)
python3 -m venv .venv && ./.venv/bin/pip install numpy
./.venv/bin/python test_svd.py
./.venv/bin/python test_mpm.py      # slow, pure Python: ~900 substeps
```

All three were last re-run from this location on 2026-09-06 and reproduce the numbers
above exactly (mass 150.00, min z 3.00, mean jp 0.7456).

`validate_wgsl.py` is the one worth running habitually — WGSL otherwise only fails at
runtime, and it takes seconds. It keeps a `_resolved_*.wgsl` file for any kernel that fails
so you can look at the flattened source; passing kernels clean up after themselves.
