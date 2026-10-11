#!/usr/bin/env python3
"""Collect the AvaFrame comparison into tables (phase 6).

For every run in extern/avaframe_sandbox/<dataset>/mpm/ with a label (batch runs, com4FlowPy,
trajectories) this runs compare_aimec.py where aimec.json is missing, then writes:

    mpm-mls-doc/avaframe/results/phase6.csv   one row per run and threshold variant
    mpm-mls-doc/avaframe/results/phase6.md    the tables for 11-avaframe-comparison.md

Usage (repository root, AvaFrame venv):

    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/summarise.py [--jobs 4] [--labels a,b]
"""
import argparse
import csv
import json
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SANDBOX = REPO / "extern" / "avaframe_sandbox"
OUT = Path(__file__).resolve().parent / "results"
DATASETS = ["avaKot", "avaGar", "avaHit", "avaWog", "avaMal"]
sys.path.insert(0, str(Path(__file__).resolve().parent))

# Display order and names of the models.
MODELS = [
    ("com4FlowPy", "com4FlowPy (AvaFrame, defaults)"),
    ("trajectories", "weBIGeo trajectories (alpha 25, theta 24, p 0.4)"),
    ("stomakhin_coulomb", "MPM Stomakhin 2013, Coulomb"),
    ("stomakhin_voellmy", "MPM Stomakhin 2013, Voellmy"),
    ("dp_coulomb", "MPM Drucker-Prager, Coulomb"),
    ("dp_voellmy", "MPM Drucker-Prager, Voellmy"),
    ("liV_coulomb", "MPM Cam Clay (Li V), Coulomb"),
    ("liV_voellmy", "MPM Cam Clay (Li V), Voellmy"),
    # Robustness: Li 2021's other cases, and the basal friction of the best variant.
    ("liI_coulomb", "MPM Cam Clay (Li I cold dense), Coulomb"),
    ("liII_coulomb", "MPM Cam Clay (Li II warm shear), Coulomb"),
    ("liIII_coulomb", "MPM Cam Clay (Li III sliding slab), Coulomb"),
    ("liIV_coulomb", "MPM Cam Clay (Li IV warm plug), Coulomb"),
    ("liV_coulomb_mu040", "MPM Cam Clay (Li V), Coulomb mu 0.40"),
    ("liV_coulomb_mu047", "MPM Cam Clay (Li V), Coulomb mu 0.47"),
    ("liV_coulomb_mu055", "MPM Cam Clay (Li V), Coulomb mu 0.55"),
]


def label_of(run_dir):
    params = json.loads((run_dir / "params.json").read_text())
    return params.get("label", "")


def latest_runs(dataset, labels):
    """The latest run per label of one dataset."""
    found = {}
    for run in sorted((SANDBOX / dataset / "mpm").glob("*")):
        if (run / "params.json").exists():
            label = label_of(run)
            if label in labels:
                found[label] = run  # sorted by time stamp, so the last one wins
    return found


def aimec(run_dir):
    import compare_aimec

    if not (run_dir / "aimec.json").exists():
        compare_aimec.compare(run_dir)
    return str(run_dir)


def fmt(value, spec="{:.0f}"):
    return "–" if value is None else spec.format(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--labels", default=",".join(m for m, _ in MODELS))
    args = parser.parse_args()
    labels = args.labels.split(",")

    runs = {ds: latest_runs(ds, labels) for ds in DATASETS}
    todo = [run for per in runs.values() for run in per.values() if not (run / "aimec.json").exists()]
    print(f"{sum(len(v) for v in runs.values())} runs, {len(todo)} need AIMEC")
    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        for done in pool.map(aimec, todo):
            print("  aimec", Path(done).name)

    OUT.mkdir(exist_ok=True)
    rows = []
    for ds in DATASETS:
        for label, name in MODELS:
            run = runs[ds].get(label)
            if run is None:
                continue
            summary = json.loads((run / "aimec.json").read_text())
            params = json.loads((run / "params.json").read_text())
            for variant, v in summary["variants"].items():
                d = v["derived"]
                rows.append({
                    "dataset": ds, "model": name, "label": label, "variant": variant, "run": run.name,
                    "runout_difference_m": d["runout_difference_m"], "at_path_end": d.get("mpm_reaches_path_end"),
                    "runout_angle_deg": d["runout_angle_mpm_deg"],
                    "runout_angle_com1DFA_deg": d["runout_angle_com1DFA_deg"], "recall": d["runout_area_recall"],
                    "precision": d["runout_area_precision"], "footprint_ha": (params.get("footprint_area_pft_gt_0.1m_m2") or params.get("footprint_area_m2") or 0) / 1e4,
                    "max_pfv_m_s": params.get("max_pfv_m_s"), "simulated_time_s": params.get("simulated_time_s"), "stopped_by": params.get("stopped_by"),
                    "wall_time_s": params.get("wall_time_s"), "gpu_time_s": params.get("gpu_time_s"),
                    "wall_contact_share": params.get("wall_contact_share"), "domain_size_m": params.get("domain_size_m"),
                })
    with open(OUT / "phase6.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    lines = []
    for ds in DATASETS:
        ref = json.loads((SANDBOX / ds / "dataset.json").read_text())["references"][0]
        lines += [f"### {ds} (reference {ref['benchmark']}: {ref['footprint_area_m2'] / 1e4:.1f} ha, max pfv {ref['max_pfv']:.0f} m/s, "
                  f"com1DFA run time {float(ref['run_time_s']):.0f} s)", "",
                  "| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) "
                  "| footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |",
                  "|---|---|---|---|---|---|---|---|---|"]
        for label, name in MODELS:
            ppr = next((r for r in rows if r["dataset"] == ds and r["label"] == label and r["variant"] == "ppr>1kPa"), None)
            pft = next((r for r in rows if r["dataset"] == ds and r["label"] == label and r["variant"] == "pft>0.1m"), None)
            if pft is None:
                continue
            lines.append(
                f"| {name} | {'≥' if ppr and ppr['at_path_end'] else ''}{fmt(ppr and ppr['runout_difference_m'], '{:+.0f}')} / "
                f"{'≥' if pft['at_path_end'] else ''}{fmt(pft['runout_difference_m'], '{:+.0f}')} "
                f"| {fmt(pft['runout_angle_deg'], '{:.1f}')} ({fmt(pft['runout_angle_com1DFA_deg'], '{:.1f}')}) "
                f"| {fmt(pft['recall'], '{:.2f}')} / {fmt(pft['precision'], '{:.2f}')} | {fmt(pft['footprint_ha'], '{:.1f}')} "
                f"| {fmt(pft['max_pfv_m_s'], '{:.0f}')} | {fmt(pft['simulated_time_s'], '{:.0f}')}{' (limit)' if pft['stopped_by'] == 'max time' else ''} "
                f"| {fmt(pft['wall_time_s'], '{:.0f}')} / {fmt(pft['gpu_time_s'], '{:.0f}')} "
                f"| {fmt(pft['wall_contact_share'] and 100 * pft['wall_contact_share'], '{:.2f} %')} |")
        lines.append("")
    # Across the datasets, per model, on the pft > 0.1 m variant (density-neutral).
    lines += ["### All five avalanches (pft > 0.1 m)", "",
              "| model | datasets | mean abs. runout difference [m] | of them at the path end (lower bound) | mean abs. runout angle difference [°] "
              "| mean recall / precision | max pfv range [m/s] |", "|---|---|---|---|---|---|---|"]
    for label, name in MODELS:
        sel = [r for r in rows if r["label"] == label and r["variant"] == "pft>0.1m"]
        if not sel:
            continue
        diff = [abs(r["runout_difference_m"]) for r in sel if r["runout_difference_m"] is not None]
        ang = [abs(r["runout_angle_deg"] - r["runout_angle_com1DFA_deg"]) for r in sel if r["runout_angle_deg"] is not None]
        rec = [r["recall"] for r in sel if r["recall"] is not None]
        pre = [r["precision"] for r in sel if r["precision"] is not None]
        pfv = [r["max_pfv_m_s"] for r in sel if r["max_pfv_m_s"] is not None]
        mean = lambda v: sum(v) / len(v) if v else None
        lines.append(f"| {name} | {len(sel)} | {fmt(mean(diff))} | {sum(1 for r in sel if r['at_path_end'])} | {fmt(mean(ang), '{:.1f}')} "
                     f"| {fmt(mean(rec), '{:.2f}')} / {fmt(mean(pre), '{:.2f}')} | {fmt(min(pfv) if pfv else None)}–{fmt(max(pfv) if pfv else None)} |")
    lines.append("")
    (OUT / "phase6.md").write_text("\n".join(lines))
    print((OUT / "phase6.md").read_text())


if __name__ == "__main__":
    main()
