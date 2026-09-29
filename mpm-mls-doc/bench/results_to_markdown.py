#!/usr/bin/env python3
"""Render bench/results/*.json as mpm-mls-doc/10-benchmark-results.md.

    python3 mpm-mls-doc/bench/results_to_markdown.py

The tables are generated so the numbers in the docs cannot drift from the checked-in result
files; add a result file, re-run, commit both. Interpretation lives in 09-performance-analysis.md.
"""
import datetime
import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "results")
OUT = os.path.join(HERE, "..", "10-benchmark-results.md")

MODEL = {"stomakhin": "Stomakhin", "dp": "Drucker–Prager", "ccc": "Cam Clay"}


def load(path):
    with open(path) as f:
        data = json.load(f)
    # M-series files hold one result; the SwiftShader ones wrap results in "runs".
    runs = data["runs"] if "runs" in data else {"current": data}
    stamp = data.get("date", "")[:10] or datetime.date.fromtimestamp(os.path.getmtime(path)).isoformat()
    return os.path.basename(path), stamp, data.get("commit", ""), runs, data.get("comparison")


def device(r):
    a = r.get("adapter", {})
    return a.get("description") or f"{a.get('vendor', '?')} {a.get('architecture', '?')}"


def row(name, label, r):
    c, p = r["config"], r["perRun"]
    stages = p["stagesMs"]
    total = sum(stages.values()) or 1.0
    if stages.get("p2g", 0) > 0:
        split = " / ".join(f"{stages.get(k, 0):.2f}" for k in ("p2g", "grid_update", "g2p")) + f" ({100 * stages['p2g'] / total:.0f} % P2G)"
    else:  # timing=pass: one query pair per chunk, no per-stage split
        split = "— (per-chunk timing)"
    return (f"| `{name}`{' ' + label if label else ''} | {MODEL.get(c['model'], c['model'])} | {c['particles']:,} | {c['gridRes']}² × {c['gridLayers']} | {c['dt']} "
            f"| {p['gpuMs']:.2f} | **{r['perSubstepGpuMs']:.3f}** | {split} | {r['simSecondsPerWallSecond']:.3g}× "
            f"| {r['memory']['solverBytes'] / 2**20:.1f} |")


HEADER = ("| file | material | particles | grid | dt | GPU ms / run | GPU ms / substep | P2G / grid / G2P ms per run | GPU-bound ceiling | solver MiB |\n"
          "|---|---|---|---|---|---|---|---|---|---|")


def main():
    files = [load(p) for p in sorted(glob.glob(os.path.join(RESULTS, "*.json")))]
    real = [f for f in files if not f[0].startswith("swiftshader")]
    soft = [f for f in files if f[0].startswith("swiftshader")]

    out = ["# Benchmark results", "",
           "*Generated from `bench/results/*.json` by `bench/results_to_markdown.py` — do not edit by hand.*",
           "*What the numbers mean, and the scenarios behind them: [09-performance-analysis.md](09-performance-analysis.md) §7–8. "
           "How to run and add results: [bench/README.md](bench/README.md).*", "",
           "Each run is `substepsPerRun` = 24 MPM substeps of the real WGSL kernels on an analytic 35° slope (4 km domain, "
           "12.5 m cells), median over the measured runs. **GPU ms** are timestamp-query times summed over the stages; "
           "the *ceiling* is simulated seconds per wall second with the GPU doing nothing else, i.e. an upper bound for the app.", ""]

    out += ["## Real GPU", ""]
    if real:
        adapter = sorted({device(r) for _, _, _, runs, _ in real for r in runs.values()})
        out += [f"Device: **Apple M5** (WebGPU adapter `{', '.join(adapter)}`), Chrome 152, 2026-09-28/29.", "", HEADER]
        for name, stamp, commit, runs, _ in real:
            for label, r in runs.items():
                out.append(row(name, "", r))
        out += ["",
                "Reading it: P2G's fixed-point atomics are ~70 % of the GPU time whatever the material; Cam Clay (the benchmark default) "
                "costs ~9 % more per substep than Stomakhin. `medium-pass` times one query pair per chunk as the app does; "
                "it agrees with the per-stage total.", ""]
    else:
        out += ["_No real-GPU results checked in yet — run the page in a browser (bench/README.md)._", ""]

    out += ["## SwiftShader (CPU Vulkan) — ratios only", "",
            "Absolute times say nothing about a GPU; these files exist for the old-vs-new comparison and the physics check "
            "(`comparison.physics`: how far the centre-of-mass trace moved between the kernels).", "", HEADER]
    for name, stamp, commit, runs, comp in soft:
        for label, r in runs.items():
            tag = f"({'baseline, ' if label == 'baseline' else ''}{r['schedule']} kernels, {commit})"
            out.append(row(name, tag, r))
    out += ["", "### Old vs new kernels", "",
            "| file | GPU speed-up | centre-of-mass travel | max trace difference |", "|---|---|---|---|"]
    for name, stamp, commit, runs, comp in soft:
        if comp:
            ph = comp["physics"]
            out.append(f"| `{name}` | **{comp['gpuSpeedup']}×** | {ph['comTravel']:.1f} m | {ph['maxComDifference'] * 1000:.1f} mm |")
    out.append("")

    with open(OUT, "w") as f:
        f.write("\n".join(out))
    print(f"wrote {os.path.normpath(OUT)}: {len(real)} real-GPU and {len(soft)} SwiftShader files")


if __name__ == "__main__":
    main()
