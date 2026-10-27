#!/usr/bin/env python3
"""Rate MPM sandbox runs against the com1DFA benchmarks with AvaFrame's AIMEC (phase 5).

AIMEC (Fischer 2013; AvaFrame ana3AIMEC) transforms both results into the avalanche path's
coordinate system and compares them along the path:
- the runout point, where the result last exceeds a threshold;
- the runout difference to the reference;
- the area indicators (true positive, false positive, false negative) on the runout area;
- the max and mean values along the path.

This script builds one AvaFrame avalanche directory per MPM run (dataset inputs, the com1DFA
benchmark as reference, the MPM rasters as the comparison module) and runs AvaFrame's own
module comparison on it, with the benchmark's own AIMEC settings (its *_AIMECCfg.ini).

Needs AvaFrame 2.1, which requires Python < 3.14 - its own venv (see README):

    /usr/bin/python3 -m venv mpm-mls-doc/avaframe/.venv-avaframe
    mpm-mls-doc/avaframe/.venv-avaframe/bin/pip install avaframe==2.1

Usage (repository root):

    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/compare_aimec.py extern/avaframe_sandbox/avaKot/mpm/<run>
    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/compare_aimec.py --latest avaKot avaGar

With --reference null|erosion|plowing the reference is the com1DFA run of that mode computed by
run_com1dfa.py (AvaFrame 2.1, extern/avaframe_sandbox/<dataset>/com1DFA/<mode>) instead of the
null-test benchmark; "auto" picks the mode the MPM run used (entrainment off: null). Then the
results go to <run>/aimec_<mode>/ and <run>/aimec_<mode>.json.

Writes <run>/aimec/ (the AvaFrame avalanche directory, with AIMEC's own plots under
Outputs/ana3AIMEC) and <run>/aimec.json, one entry per threshold variant:
- ppr > 1 kPa: AvaFrame's default runout criterion;
- pft > 0.1 m: the flow thickness, which ignores the MPM's sub-centimetre layer of single particles.
"""
import argparse
import configparser
import hashlib
import json
import logging
import math
import shutil
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
AVAFRAME_SRC = REPO / "extern" / "avaframe"

# Threshold variants: (label, runout result type, threshold in its unit)
VARIANTS = [("ppr>1kPa", "ppr", 1.0), ("pft>0.1m", "pft", 0.1)]


def meta_benchmark(meta, scenario):
    """The null-test benchmark of a scenario (its AIMEC settings serve every reference)."""
    return next(r["benchmark"] for r in meta["references"] if r["release_scenario"] == scenario)


def reference_for(dataset_dir, scenario):
    meta = json.loads((dataset_dir / "dataset.json").read_text())
    for ref in meta["references"]:
        if ref["release_scenario"] == scenario:
            return meta, ref
    return meta, None


ENTRAINMENT_MODES = {0: "null", 1: "plowing", 2: "erosion"}


def reference_mode(run_dir, reference):
    """None for the null-test benchmark, else the run_com1dfa.py mode to compare with."""
    if reference == "benchmark":
        return None
    if reference != "auto":
        return reference
    params = json.loads((run_dir / "params.json").read_text())
    mode = params.get("solver_settings", {}).get("entrainment_mode", 0)
    return ENTRAINMENT_MODES.get(mode, "null")


def build_avalanche_dir(run_dir, label, runout_res_type, threshold, mode=None):
    """Lays out an AvaFrame avalanche directory for one comparison and returns (path, cfg file)."""
    params = json.loads((run_dir / "params.json").read_text())
    name, scenario = params["dataset"], params["release_scenario"]
    dataset_dir = run_dir.parents[1]
    meta, ref = reference_for(dataset_dir, scenario)
    if ref is None:
        raise SystemExit(f"{run_dir}: no com1DFA benchmark for {name}/{scenario}")
    if mode is not None:
        own = dataset_dir / "com1DFA" / mode
        summary = json.loads((own / "summary.json").read_text())
        if summary["release_scenario"] != scenario:
            raise SystemExit(f"{run_dir}: the com1DFA {mode} run is for {summary['release_scenario']}, not {scenario}")
        ref = dict(ref, benchmark=f"com1DFA 2.1 {mode}", friction_model=summary["report"].get("Friction model"),
                   density=summary["report"].get("Density [kgm-3]"), program_version="2.1 (PyPI)",
                   run_time_s=summary["report"].get("Process run time [s]"),
                   entrained_volume_m3=summary["report"].get("Entrained volume [m3]"), peak_dir=own / "Outputs" / "com1DFA" / "peakFiles")

    ava = run_dir / ("aimec" if mode is None else f"aimec_{mode}") / label.replace(">", "_gt_")
    if ava.exists():
        shutil.rmtree(ava)
    # Inputs: AvaFrame's own, as com1DFA saw them (DEM, release, path, split point).
    shutil.copytree(AVAFRAME_SRC / meta["source"]["inputs"], ava / "Inputs")
    # Reference: the benchmark rasters under their original names (AvaFrame parses them).
    bench = AVAFRAME_SRC / "benchmarks" / meta_benchmark(meta, scenario)
    ref_dir = ava / "Outputs" / "com1DFA" / "peakFiles"
    ref_dir.mkdir(parents=True)
    for f in (ref["peak_dir"] if mode is not None else bench).glob("*_p??.asc"):
        shutil.copy(f, ref_dir / f.name)
    # Comparison: the MPM rasters, named like a com1DFA result so AvaFrame's parser accepts them.
    sim_hash = hashlib.sha1(run_dir.name.encode()).hexdigest()[:10]
    mpm_dir = ava / "Outputs" / "mpm" / "peakFiles"
    mpm_dir.mkdir(parents=True)
    for res in ("pft", "pfv", "ppr"):
        shutil.copy(run_dir / f"{res}.asc", mpm_dir / f"{scenario}_{sim_hash}_C_S_null_dfa_{res}.asc")

    # AIMEC settings: the benchmark's own file, then the comparison setup.
    cfg = configparser.ConfigParser()
    cfg.optionxform = str
    bench_cfg = next(bench.glob("*_AIMECCfg.ini"), None)
    if bench_cfg:
        cfg.read(bench_cfg)
    for section in ("AIMECSETUP", "PLOTS", "FLAGS"):
        if not cfg.has_section(section):
            cfg.add_section(section)
    cfg["AIMECSETUP"]["comModules"] = "com1DFA|mpm"
    cfg["AIMECSETUP"]["runoutResType"] = runout_res_type
    cfg["AIMECSETUP"]["thresholdValue"] = str(threshold)
    cfg["AIMECSETUP"]["resTypes"] = "ppr|pft|pfv"
    cfg["FLAGS"]["flagMass"] = "False"  # the MPM writes no com1DFA mass log
    cfg_file = ava / "aimecCfg.ini"
    with open(cfg_file, "w") as f:
        cfg.write(f)
    return ava, cfg_file, params, ref


def run_aimec(ava, cfg_file):
    from avaframe.ana3AIMEC import ana3AIMEC, aimecTools, dfa2Aimec
    from avaframe.in3Utils import cfgUtils
    from avaframe.out3Plot import outAIMEC

    # AIMEC draws its runout-statistics figure unconditionally, and matplotlib raises on it for
    # some result pairs (NaN colour-bar limits, Madlein vs com1DFA-Coulomb). The figure is only
    # a picture; the numbers do not depend on it.
    if not getattr(outAIMEC.visuRunoutStat, "_tolerant", False):
        plot = outAIMEC.visuRunoutStat

        def tolerant(*args, **kwargs):
            try:
                return plot(*args, **kwargs)
            except ValueError as error:
                logging.warning("AIMEC runout statistics figure skipped: %s", error)
                return None

        tolerant._tolerant = True
        outAIMEC.visuRunoutStat = tolerant

    cfg = cfgUtils.getModuleConfig(ana3AIMEC, fileOverride=cfg_file, toPrint=False)
    setup = cfg["AIMECSETUP"]
    inputs_df, path_dict = dfa2Aimec.dfaBench2Aimec(ava, cfg)
    path_dict = aimecTools.readAIMECinputs(ava, path_dict, setup.getboolean("defineRunoutArea"), dirName="com1DFA_mpm")
    transfo, results, _, _ = ana3AIMEC.mainAIMEC(path_dict, inputs_df, cfg)
    # The analysis ends with the AIMEC path: a result above the threshold in the last cross
    # section has a runout of "at least" there, not a measured one.
    s = transfo["s"]
    return results, path_dict["refSimRowHash"], float(s[-1]), float(s[-1] - s[-2])


def to_plain(value):
    if isinstance(value, (np.floating, float)):
        return None if math.isnan(float(value)) else float(value)
    if isinstance(value, (np.integer, int)):
        return int(value)
    if isinstance(value, (np.bool_, bool)):
        return bool(value)
    return value if isinstance(value, str) else None


# AIMEC result columns worth keeping; the rest are arrays along the path.
KEEP = ["sRunout", "xRunout", "yRunout", "deltaSXY", "runoutAngle", "zRelease", "zRunout", "sMeanRunout",
        "TP", "FN", "FP", "TN", "maxpprCrossMax", "maxpftCrossMax", "maxpfvCrossMax",
        "pprFieldMax", "pftFieldMax", "pfvFieldMax", "pprFieldMean", "pftFieldMean", "pfvFieldMean"]


def compare(run_dir, reference="benchmark"):
    run_dir = run_dir.resolve()
    mode = reference_mode(run_dir, reference)
    summary = {"run": str(run_dir.relative_to(REPO)), "variants": {}}
    for label, res_type, threshold in VARIANTS:
        ava, cfg_file, params, ref = build_avalanche_dir(run_dir, label, res_type, threshold, mode)
        results, ref_hash, path_end, ds = run_aimec(ava, cfg_file)
        rows = {}
        for row_hash, row in results.iterrows():
            key = "com1DFA" if row_hash == ref_hash else "mpm"
            rows[key] = {k: to_plain(row[k]) for k in KEEP if k in row.index}
        mpm, com = rows.get("mpm", {}), rows.get("com1DFA", {})
        tp, fn, fp = mpm.get("TP") or 0.0, mpm.get("FN") or 0.0, mpm.get("FP") or 0.0
        # sRunout: path coordinate of the last cross section above the threshold. deltaSXY is
        # each result's own length along the path (first to last such section), not a difference.
        # TP/FN/FP: areas in the runout area only (from startOfRunoutAreaAngle on), reference = com1DFA.
        s_mpm, s_com = mpm.get("sRunout"), com.get("sRunout")
        derived = {
            "runout_s_mpm_m": s_mpm,
            "runout_s_com1DFA_m": s_com,
            "runout_difference_m": s_mpm - s_com if s_mpm is not None and s_com is not None else None,
            "path_length_mpm_m": mpm.get("deltaSXY"),
            "path_length_com1DFA_m": com.get("deltaSXY"),
            "runout_angle_mpm_deg": mpm.get("runoutAngle"),
            "runout_angle_com1DFA_deg": com.get("runoutAngle"),
            "runout_area_recall": tp / (tp + fn) if tp + fn > 0 else None,
            "runout_area_precision": tp / (tp + fp) if tp + fp > 0 else None,
            "runout_area_fp_over_ref": fp / (tp + fn) if tp + fn > 0 else None,
            "path_end_s_m": path_end,
            "mpm_reaches_path_end": s_mpm is not None and s_mpm >= path_end - 1.5 * ds,
            "com1DFA_reaches_path_end": s_com is not None and s_com >= path_end - 1.5 * ds,
        }
        summary["variants"][label] = {"threshold": f"{res_type} > {threshold}", "derived": derived, "mpm": mpm, "com1DFA": com,
                                      "avalanche_dir": str(ava.relative_to(REPO))}
        fmt = lambda v, f="{:.1f}": "n/a" if v is None else f.format(v)
        print(f"  {label}: runout s MPM {fmt(s_mpm)} m / com1DFA {fmt(s_com)} m (difference {fmt(derived['runout_difference_m'])} m), "
              f"runout angle {fmt(derived['runout_angle_mpm_deg'])} / {fmt(derived['runout_angle_com1DFA_deg'])} deg, runout area "
              f"recall {fmt(derived['runout_area_recall'], '{:.2f}')}, precision {fmt(derived['runout_area_precision'], '{:.2f}')}")
    summary["mpm_params"] = {k: params.get(k) for k in ("dataset", "release_scenario", "material_preset", "label", "simulated_time_s",
                                                        "stopped_by", "max_pft_m", "max_pfv_m_s", "energy_line_mu_eff", "gpu_ms_per_substep")}
    summary["mpm_params"]["basal_friction_model"] = params.get("solver_settings", {}).get("basal_friction_model")
    summary["mpm_params"]["entrainment"] = params.get("entrainment")
    summary["reference"] = {k: ref.get(k) for k in ("benchmark", "friction_model", "density", "program_version", "run_time_s", "entrained_volume_m3")}
    (run_dir / ("aimec.json" if mode is None else f"aimec_{mode}.json")).write_text(json.dumps(summary, indent=2))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("runs", nargs="*", help="run folders, extern/avaframe_sandbox/<dataset>/mpm/<run>")
    parser.add_argument("--latest", nargs="*", default=[], help="the latest run of each named dataset")
    parser.add_argument("--reference", default="benchmark", choices=["benchmark", "null", "erosion", "plowing", "coulomb", "auto"],
                        help="the null-test benchmark (default), or a com1DFA run from run_com1dfa.py")
    args = parser.parse_args()
    logging.basicConfig(level=logging.WARNING)
    runs = [Path(r) for r in args.runs]
    for name in args.latest:
        candidates = sorted((REPO / "extern" / "avaframe_sandbox" / name / "mpm").glob("*"))
        if candidates:
            runs.append(candidates[-1])
    if not runs:
        parser.error("name run folders or use --latest <dataset>")
    for run in runs:
        print(run)
        compare(run, args.reference)


if __name__ == "__main__":
    sys.exit(main())
