#!/usr/bin/env python3
"""Run com1DFA (AvaFrame 2.1) on a dataset, with or without entrainment (entrainment comparison).

The com1DFA references in the datasets are AvaFrame's null-test benchmarks: no entrainment,
computed with AvaFrame 1.11. For the entrainment comparison this computes com1DFA anew with
AvaFrame 2.1 in three modes, on the same inputs and with the benchmark's own settings:

    null      no entrainment - the same version as the other two, as their baseline
    coulomb   no entrainment, Coulomb friction mu 0.47 (the MPM's basal law) instead of samosAT
    erosion   entrainment, com1DFA's default: erosion at tau_b |u| / e_b (entEroEnergy 5000 J/kg)
    plowing   entrainment, frontal plowing (entEroEnergy 0)

The entrainment area is the dataset's ENT shapefile (0.3 m), the release the comparison's
scenario. Results go to extern/avaframe_sandbox/<dataset>/com1DFA/<mode>/: the AvaFrame
avalanche directory (Inputs, Outputs with peak files and mass log) and summary.json with
com1DFA's report (initial, final and entrained mass and volume, run time).

Settings: the benchmark's com1DFACfg.ini, applied over AvaFrame 2.1's defaults where 2.1 still
has the key (samosAT friction, rho 200 kg/m^3, the stop criterion, ...).

Usage (repository root, AvaFrame venv, see compare_aimec.py):

    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/run_com1dfa.py avaKot --modes null erosion plowing
"""
import argparse
import configparser
import json
import logging
import shutil
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
AVAFRAME_SRC = REPO / "extern" / "avaframe"
SANDBOX = REPO / "extern" / "avaframe_sandbox"

# The comparison's release scenario and null benchmark per dataset (as in batch_phase6.json).
DATASETS = {
    "avaKot": ("relKot", "avaKotNullTest"),
    "avaGar": ("relGar", "avaGarNullTest"),
    "avaHit": ("relHit", "avaHitNullTest"),
    "avaWog": ("relWog", "avaWogNullTest"),
    "avaMal": ("relMal1to3", "avaMalNullTest"),
}
MODES = {
    "null": {"simTypeList": "null"},
    "erosion": {"simTypeList": "ent", "entEroEnergy": "5000"},
    "plowing": {"simTypeList": "ent", "entEroEnergy": "0"},
    # Not entrainment: com1DFA with the MPM's basal law (Coulomb, mu 0.47), to tell a difference
    # in the friction law from one in the numerics.
    "coulomb": {"simTypeList": "null", "frictModel": "Coulomb", "mucoulomb": "0.47"},
}


def build_avalanche_dir(name, scenario, mode):
    ava = SANDBOX / name / "com1DFA" / mode
    if ava.exists():
        shutil.rmtree(ava)
    src = AVAFRAME_SRC / "avaframe" / "data" / name / "Inputs"
    (ava / "Inputs" / "REL").mkdir(parents=True)
    shutil.copy(next(src.glob("*.asc")), ava / "Inputs")
    for f in (src / "REL").glob(f"{scenario}.*"):
        shutil.copy(f, ava / "Inputs" / "REL" / f.name)
    for sub in ("LINES", "POINTS") + (("ENT",) if mode != "null" else ()):
        shutil.copytree(src / sub, ava / "Inputs" / sub)
    return ava


def module_config(benchmark, scenario, mode):
    """2.1's defaults, the benchmark's values where 2.1 has the key, then the mode."""
    from avaframe.com1DFA import com1DFA
    from avaframe.in3Utils import cfgUtils

    cfg = cfgUtils.getModuleConfig(com1DFA, toPrint=False)
    bench = configparser.ConfigParser()
    bench.optionxform = str
    bench.read(next((AVAFRAME_SRC / "benchmarks" / benchmark).glob("*_com1DFACfg.ini")))
    skipped = []
    for section in bench.sections():
        for key, value in bench[section].items():
            if cfg.has_section(section) and cfg.has_option(section, key):
                cfg[section][key] = value
            else:
                skipped.append(f"{section}.{key}")
    cfg["INPUT"]["releaseScenario"] = scenario
    cfg["GENERAL"]["initialiseParticlesFromFile"] = "False"
    for key, value in MODES[mode].items():
        cfg["GENERAL"][key] = value
    return cfg, skipped


def run(name, mode):
    from avaframe.com1DFA import com1DFA
    from avaframe.in3Utils import cfgUtils

    scenario, benchmark = DATASETS[name]
    ava = build_avalanche_dir(name, scenario, mode)
    cfg_main = cfgUtils.getGeneralConfig()
    cfg_main["MAIN"]["avalancheDir"] = str(ava)
    cfg_main["FLAGS"]["showPlot"] = "False"
    cfg_main["FLAGS"]["savePlot"] = "False"
    cfg, skipped = module_config(benchmark, scenario, mode)
    started = time.time()
    _, _, reports, _ = com1DFA.com1DFAMain(cfg_main, cfgInfo=cfg)
    wall = time.time() - started
    report = reports[0] if reports else {}
    params = report.get("Simulation Parameters", {})
    summary = {"dataset": name, "release_scenario": scenario, "mode": mode, "benchmark_settings": benchmark,
               # The report's "Program version" comes from git describe of the working directory, not
               # AvaFrame; the venv has AvaFrame 2.1 from PyPI.
               "avaframe": "2.1 (PyPI)",
               "wall_time_s": round(wall, 1), "settings_not_in_2.1": skipped, "report": params,
               "peak_files": sorted(str(p.relative_to(SANDBOX / name)) for p in (ava / "Outputs" / "com1DFA" / "peakFiles").glob("*.asc"))}
    (ava / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    print(f"  {mode}: entrained {params.get('Entrained volume [m3]', '?')} m3 ({params.get('Entrained mass [kg]', '?')} kg), "
          f"run time {params.get('Process run time [s]', '?')} s, computed in {wall:.0f} s")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("datasets", nargs="*", default=list(DATASETS), help="dataset names (default: all five)")
    parser.add_argument("--modes", nargs="+", default=list(MODES), choices=list(MODES))
    args = parser.parse_args()
    logging.basicConfig(level=logging.WARNING)
    for name in args.datasets:
        print(name)
        for mode in args.modes:
            run(name, mode)


if __name__ == "__main__":
    sys.exit(main())
