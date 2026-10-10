#!/usr/bin/env python3
"""Run com4FlowPy (AvaFrame) on a sandbox dataset and store it like an MPM run (phase 6).

com4FlowPy (D'Amboise et al. 2022, Flow-Py) is AvaFrame's data-driven runout model: random-walk
routing with an energy-line (alpha angle) stop. It is a second reference next to com1DFA, and
what the earlier weBIGeo evaluation compared its trajectory model with.

Its outputs are not com1DFA's peak fields, so they are mapped for AIMEC:
    pft.asc  footprint flag, 1 where cellCounts > 0 (no flow thickness in the model) - compare
             footprints only (the pft > 0.1 m variant)
    pfv.asc  sqrt(2 g zDelta) [m/s], zDelta = height of the energy line above the terrain (a
             kinetic-energy height); the same conversion weBIGeo's trajectory shader uses
    ppr.asc  rho pfv^2 / 1000 [kPa], rho = 200 kg/m^3 like the com1DFA benchmarks

into extern/avaframe_sandbox/<dataset>/mpm/<time>_<release>_com4FlowPy/, with params.json, so
compare_aimec.py treats it like any other run. Default parameters (alpha 25, exponent 8,
flux threshold 3e-4), as in the earlier weBIGeo evaluation ("com4FlowPy with default
parameters").

Usage (repository root, AvaFrame venv):

    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/run_com4flowpy.py avaKot avaGar
"""
import argparse
import datetime
import json
import shutil
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
AVAFRAME_SRC = REPO / "extern" / "avaframe"
SANDBOX = REPO / "extern" / "avaframe_sandbox"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_dataset as P  # noqa: E402  (ESRI ASCII reader/writer)

DENSITY = 200.0
G = 9.81


def read_tif(path):
    import rasterio

    with rasterio.open(path) as src:
        data = src.read(1).astype(float)
        nodata = src.nodata
        if nodata is not None:
            data[data == nodata] = np.nan
        return data, src.transform


def run(name, scenario=None, reuse=False):
    meta = json.loads((SANDBOX / name / "dataset.json").read_text())
    scenario = scenario or meta["release_scenarios"][0]["name"]
    ava = SANDBOX / name / "com4FlowPy"
    if reuse and any((ava / "Outputs" / "com4FlowPy").rglob("*cellCounts*.tif")):
        return convert(name, scenario, ava, wall=None)
    if ava.exists():
        shutil.rmtree(ava)
    # Inputs: the DEM and only this release scenario (com4FlowPy takes every REL file it finds).
    src_inputs = AVAFRAME_SRC / meta["source"]["inputs"]
    (ava / "Inputs" / "REL").mkdir(parents=True)
    shutil.copy(src_inputs / meta["dem"]["source"], ava / "Inputs" / meta["dem"]["source"])
    for f in (src_inputs / "REL").glob(scenario + ".*"):
        shutil.copy(f, ava / "Inputs" / "REL" / f.name)

    from avaframe import runCom4FlowPy

    started = datetime.datetime.now()
    runCom4FlowPy.main(avalancheDir=str(ava))
    wall = (datetime.datetime.now() - started).total_seconds()
    return convert(name, scenario, ava, wall)


def convert(name, scenario, ava, wall):
    out = ava / "Outputs" / "com4FlowPy"
    cell_counts = sorted(out.rglob("*cellCounts*.tif"))[-1]
    z_delta = sorted(out.rglob("*zdelta*.tif"))[-1]  # AvaFrame writes it lower case
    counts, _ = read_tif(cell_counts)
    zd, _ = read_tif(z_delta)
    dem = P.read_asc(SANDBOX / name / "dem.asc")
    if counts.shape != dem.data.shape:
        raise SystemExit(f"{name}: com4FlowPy grid {counts.shape} differs from the DEM {dem.data.shape}")
    counts = np.nan_to_num(counts)
    zd = np.nan_to_num(zd)
    pft = (counts > 0).astype(float)
    pfv = np.sqrt(2.0 * G * np.maximum(zd, 0.0)) * pft
    ppr = DENSITY * pfv**2 / 1000.0

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = SANDBOX / name / "mpm" / f"{stamp}_{scenario}_com4FlowPy"
    run_dir.mkdir(parents=True)
    for res, values in (("pft", pft), ("pfv", pfv), ("ppr", ppr)):
        P.write_asc(run_dir / f"{res}.asc", P.Grid(values, dem.xll, dem.yll, dem.cellsize))
    cell = dem.cellsize ** 2
    params = {
        "dataset": name, "release_scenario": scenario, "model": "com4FlowPy", "label": "com4FlowPy",
        "material_preset": "com4FlowPy (AvaFrame 2.1, default: alpha 25, exponent 8, flux threshold 3e-4)",
        "wall_time_s": wall, "footprint_area_m2": float(pft.sum() * cell), "max_pfv_m_s": float(pfv.max()),
        "conventions": "pft = footprint flag (cellCounts > 0), pfv = sqrt(2 g zDelta), ppr = 200 pfv^2 / 1000",
        "source": str(out.relative_to(REPO)),
    }
    (run_dir / "params.json").write_text(json.dumps(params, indent=2))
    print(f"{name}/{scenario}: com4FlowPy in {wall or 0:.0f} s, footprint {pft.sum() * cell / 1e4:.1f} ha -> {run_dir.relative_to(REPO)}")
    return run_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("datasets", nargs="+")
    parser.add_argument("--reuse", action="store_true", help="convert an existing com4FlowPy output instead of running it again")
    args = parser.parse_args()
    for name in args.datasets:
        run(name, reuse=args.reuse)


if __name__ == "__main__":
    main()
