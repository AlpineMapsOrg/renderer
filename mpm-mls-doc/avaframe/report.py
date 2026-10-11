#!/usr/bin/env python3
"""Build the detailed AvaFrame comparison report: one HTML page with images (phase 6).

For every run in results/phase6.csv (the latest run per label, see summarise.py) it collects:
- the app's screenshots, if the batch took them: the 2D sandbox view with the MPM peak thickness
  (sandbox.png, cropped to the sandbox window) and the map with the overlay (map.png, cropped to
  the map);
- a standard comparison image drawn here from the exported rasters, the same for every model
  including com4FlowPy and the trajectory model: hillshade, release (cyan), the model's
  footprint coloured by peak flow thickness (pft > 0.1 m; a flag for the footprint-only
  models), and com1DFA's footprint outline (black);
- the AIMEC numbers of both threshold variants.

Writes extern/avaframe_sandbox/report/index.html and images/; publish that folder.

Usage (repository root, after summarise.py):

    mpm-mls-doc/avaframe/.venv-avaframe/bin/python mpm-mls-doc/avaframe/report.py
"""
import csv
import html
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SANDBOX = REPO / "extern" / "avaframe_sandbox"
OUT = SANDBOX / "report"
sys.path.insert(0, str(HERE))
import prepare_dataset as P  # noqa: E402
from summarise import DATASETS, MODELS  # noqa: E402

# Crop boxes as fractions of the app window (default layout: sidebar on the right, the sandbox
# window centred near the top).
SANDBOX_CROP = (0.315, 0.090, 0.685, 0.795)
MAP_CROP = (0.0, 0.035, 0.745, 1.0)
VIRIDIS = np.array([[68, 1, 84], [59, 82, 139], [33, 145, 140], [94, 201, 98], [253, 231, 37]], float)

NAMES = {label: name for label, name in MODELS}
GROUPS = [
    ("Reference models", ["com4FlowPy", "trajectories"]),
    ("MPM: three materials, Coulomb and Voellmy", ["stomakhin_coulomb", "stomakhin_voellmy", "dp_coulomb", "dp_voellmy", "liV_coulomb", "liV_voellmy"]),
    ("MPM: Li 2021 cases I–IV (Coulomb)", ["liI_coulomb", "liII_coulomb", "liIII_coulomb", "liIV_coulomb"]),
    ("MPM: basal friction of Li V (Coulomb)", ["liV_coulomb_mu040", "liV_coulomb_mu047", "liV_coulomb", "liV_coulomb_mu055"]),
]


def viridis(f):
    f = np.clip(f, 0.0, 1.0) * 4.0
    i = np.minimum(f.astype(int), 3)
    t = (f - i)[..., None]
    return VIRIDIS[i] * (1 - t) + VIRIDIS[i + 1] * t


def comparison_image(dataset, run_dir, footprint_only, path):
    root = SANDBOX / dataset
    dem = P.read_asc(root / "dem.asc")
    meta = json.loads((root / "dataset.json").read_text())
    ref = P.read_asc(root / meta["references"][0]["files"]["pft"])
    ref_mask = np.nan_to_num(P.resample_nearest(ref, dem)) > 0
    pft = np.nan_to_num(P.read_asc(run_dir / "pft.asc").data)
    release = np.nan_to_num(P.read_asc(root / meta["release_scenarios"][0]["file"]).data) > 0
    shade = np.nan_to_num(P.hillshade(dem), nan=0.25)
    rgb = np.stack([shade] * 3, -1) * 235 + 20
    mask = pft > (0.0 if footprint_only else 0.1)
    colours = np.tile(np.array([[230.0, 120.0, 30.0]]), (mask.sum(), 1)) if footprint_only else viridis(pft[mask] / 5.0)
    rgb[mask] = rgb[mask] * 0.25 + colours * 0.75
    rgb[release] = rgb[release] * 0.3 + np.array([25, 215, 240]) * 0.7
    rgb[P.outline(ref_mask)] = [10, 10, 10]
    img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))
    img.thumbnail((640, 640))
    img.save(path, quality=85)


def crop_screenshot(src, box, path, width):
    img = Image.open(src).convert("RGB")
    w, h = img.size
    img = img.crop((int(box[0] * w), int(box[1] * h), int(box[2] * w), int(box[3] * h)))
    img.thumbnail((width, width))
    img.save(path, quality=80)


def fmt(v, spec="{:.0f}", dash="–"):
    if v in (None, "", "None"):
        return dash
    return spec.format(float(v))


def main():
    rows = list(csv.DictReader(open(HERE / "results" / "phase6.csv")))
    (OUT / "images").mkdir(parents=True, exist_ok=True)
    by = {(r["dataset"], r["label"], r["variant"]): r for r in rows}
    summary_md = (HERE / "results" / "phase6.md").read_text()

    # Summary table across avalanches (from the markdown summary).
    summary = summary_md[summary_md.index("| model | datasets"):].strip().splitlines()
    head = [c.strip() for c in summary[0].strip("|").split("|")]
    body = [[c.strip() for c in l.strip("|").split("|")] for l in summary[2:]]

    cards = {}
    for ds in DATASETS:
        for label, _ in MODELS:
            r = by.get((ds, label, "pft>0.1m"))
            if r is None:
                continue
            run_dir = SANDBOX / ds / "mpm" / r["run"]
            stem = f"{ds}_{label}"
            footprint_only = label in ("com4FlowPy", "trajectories")
            comparison_image(ds, run_dir, footprint_only, OUT / "images" / f"{stem}_plot.jpg")
            shots = {}
            for name, box, width in (("sandbox", SANDBOX_CROP, 720), ("map", MAP_CROP, 900)):
                if (run_dir / f"{name}.png").exists():
                    crop_screenshot(run_dir / f"{name}.png", box, OUT / "images" / f"{stem}_{name}.jpg", width)
                    shots[name] = f"images/{stem}_{name}.jpg"
            params = json.loads((run_dir / "params.json").read_text())
            cards[(ds, label)] = (r, by.get((ds, label, "ppr>1kPa")), shots, f"images/{stem}_plot.jpg", params)
            print("  ", stem, "+".join(shots) or "plot only")

    refs = {ds: json.loads((SANDBOX / ds / "dataset.json").read_text()) for ds in DATASETS}
    e = html.escape
    out = [PAGE_HEAD]
    out.append('<header class="masthead"><p class="eyebrow">weBIGeo · MLS-MPM avalanche · VisComp WS26</p>'
               '<h1>AvaFrame comparison report</h1>'
               '<p class="lede">The MLS-MPM solver against AvaFrame\'s com1DFA benchmarks, com4FlowPy and weBIGeo\'s earlier trajectory model, '
               'on five real Austrian avalanches. All models run on AvaFrame\'s own terrain and are rated with AvaFrame\'s AIMEC.</p>'
               f'<p class="meta">{len(cards)} runs · 5 avalanches · AvaFrame 2.1 · rated against com1DFA v1.11 benchmarks</p></header>')
    out.append('<nav class="toc" aria-label="Sections"><a href="#summary">Summary</a><a href="#findings">Findings</a><a href="#setup">Setup</a><a href="#robustness">Robustness</a>'
               + "".join(f'<a href="#{ds}">{ds}</a>' for ds in DATASETS) + '<a href="#limits">Limits</a></nav>')

    out.append('<section id="summary"><h2>Summary across the five avalanches</h2>'
               '<p>Flow thickness above 0.1 m, so the MPM\'s sub-centimetre layer of single particles does not count. "Runout difference" '
               'is along the avalanche path, model minus com1DFA. "At the path end" counts runs whose runout passes the end of AvaFrame\'s '
               'AIMEC path: their difference is a lower bound.</p><div class="scroll"><table class="data"><thead><tr>'
               + "".join(f"<th>{e(h)}</th>" for h in head) + "</tr></thead><tbody>"
               + "".join("<tr>" + "".join(f"<td>{e(c)}</td>" for c in row) + "</tr>" for row in body)
               + "</tbody></table></div></section>")

    # Key findings, with the numbers from the summary rows (label -> mean runout, mean angle, speeds).
    by_name = {row[0]: row for row in body}
    m = {label: by_name[name] for label, name in MODELS if name in by_name}
    def k(label, col):  # col 2: mean abs runout [m], 4: mean abs angle [deg], 6: max pfv range
        return m[label][col] if label in m else "–"
    out.append(f"""<section id="findings"><h2>Findings</h2><ul>
<li><strong>Closest to com1DFA: Cam Clay with Li 2021's cold dense parameters (case I) and Coulomb friction.</strong> Mean runout difference {k("liI_coulomb", 2)} m, travel angle {k("liI_coulomb", 4)}° off on average. Next: Li V at μ 0.47 ({k("liV_coulomb_mu047", 2)} m, {k("liV_coulomb_mu047", 4)}°) and Li V at its preset μ 0.49 ({k("liV_coulomb", 2)} m, {k("liV_coulomb", 4)}°). The data-driven models: com4FlowPy {k("com4FlowPy", 2)} m / {k("com4FlowPy", 4)}°, weBIGeo trajectories {k("trajectories", 2)} m / {k("trajectories", 4)}°.</li>
<li><strong>The material decides whether the flow reaches the runout zone at all.</strong> Li's cohesive regimes (II warm shear, III sliding slab, IV warm plug) stop far too early: {k("liII_coulomb", 2)}, {k("liIII_coulomb", 2)} and {k("liIV_coulomb", 2)} m on average, missing most of com1DFA's runout zone.</li>
<li><strong>One Coulomb μ cannot fit all five avalanches.</strong> For Li V the mean runout difference is {k("liV_coulomb_mu040", 2)} m at μ 0.40, {k("liV_coulomb_mu047", 2)} m at 0.47 and {k("liV_coulomb_mu055", 2)} m at 0.55. Per avalanche, though, μ moves the runout by hundreds of metres. The avalanches with com1DFA's steeper travel angles (Kontertal, Gallreide, Hintertux; 28–31°) fit best at μ 0.55, and the two with the flattest (Wolfsgruben, Madlein; about 25.5°) fit best at μ 0.40. Coulomb friction gives roughly one travel angle; com1DFA's depth- and speed-dependent samosAT gives each avalanche its own.</li>
<li><strong>Speed is the open problem.</strong> The best-matching variants reach {k("liI_coulomb", 6)} m/s (Li I) against com1DFA's 40–53 m/s: Coulomb friction has no velocity-dependent resistance. The cohesive regimes are slower ({k("liIII_coulomb", 6)} m/s for Li III) but stop short. No variant gets both runout and speed right.</li>
<li><strong>Voellmy with com1DFA's parameters (μ 0.155, ξ 4000) runs too far for every material:</strong> {k("liV_voellmy", 2)} m on average for Li V, with three of five runs past the end of the AIMEC path. Its drag hardly acts in the MPM. The flow depth h in the drag term is the first suspect.</li>
<li><strong>Stomakhin's film snow</strong> has realistic speeds ({k("stomakhin_coulomb", 6)} m/s) but stops early on the long avalanches (mean {k("stomakhin_coulomb", 2)} m).</li>
</ul></section>""")
    out.append(SETUP_HTML)

    # Robustness tables: material (Li I–V) and mu.
    def robust_table(title, labels, note):
        t = [f'<h3>{e(title)}</h3><p>{note}</p><div class="scroll"><table class="data"><thead><tr><th>variant</th>']
        t += [f"<th>{ds}</th>" for ds in DATASETS] + ["</tr></thead><tbody>"]
        for label in labels:
            t.append(f"<tr><td>{e(NAMES[label])}</td>")
            for ds in DATASETS:
                r = by.get((ds, label, "pft>0.1m"))
                if r is None:
                    t.append("<td>–</td>")
                    continue
                ge = "≥" if r["at_path_end"] == "True" else ""
                t.append(f'<td><span class="num">{ge}{fmt(r["runout_difference_m"], "{:+.0f}")} m</span><span class="sub">'
                         f'{fmt(r["runout_angle_deg"], "{:.1f}")}° / {fmt(r["runout_angle_com1DFA_deg"], "{:.1f}")}°</span></td>')
            t.append("</tr>")
        t.append("</tbody></table></div>")
        return "".join(t)

    out.append('<section id="robustness"><h2>Robustness: material and friction</h2>'
               '<p>Each cell: runout difference to com1DFA, then the travel angle of the model and of com1DFA.</p>'
               + robust_table("Li 2021 cases I–V, Coulomb", ["liI_coulomb", "liII_coulomb", "liIII_coulomb", "liIV_coulomb", "liV_coulomb"],
                              "Li et al. 2021's five parameter sets for Cohesive Cam Clay: four flow regimes and the Vallée de la Sionne back-calculation. Basal μ 0.47 (I–IV) and 0.49 (V).")
               + robust_table("Basal friction μ for Li V, Coulomb", ["liV_coulomb_mu040", "liV_coulomb_mu047", "liV_coulomb", "liV_coulomb_mu055"],
                              "Same material, basal friction coefficient 0.40, 0.47, 0.49 (the preset) and 0.55.")
               + "</section>")

    for ds in DATASETS:
        ref = refs[ds]["references"][0]
        out.append(f'<section id="{ds}" class="dataset"><h2>{ds}</h2><p class="meta">Reference {e(ref["benchmark"])}: '
                   f'footprint {ref["footprint_area_m2"] / 1e4:.1f} ha, max flow thickness {ref["max_pft"]:.1f} m, max speed {ref["max_pfv"]:.0f} m/s, '
                   f'run time {float(ref["run_time_s"]):.0f} s. Release {e(refs[ds]["release_scenarios"][0]["name"])}, '
                   f'{refs[ds]["release_scenarios"][0]["volume_m3"]:.0f} m³.</p>')
        for group, labels in GROUPS:
            present = [l for l in labels if (ds, l) in cards]
            if not present:
                continue
            out.append(f"<h3>{e(group)}</h3><div class='cards'>")
            for label in present:
                r, rp, shots, plot, params = cards[(ds, label)]
                ge = "≥" if r["at_path_end"] == "True" else ""
                gp = "≥" if rp and rp["at_path_end"] == "True" else ""
                stats = [
                    ("Runout diff. pft / ppr", f'{ge}{fmt(r["runout_difference_m"], "{:+.0f}")} / {gp}{fmt(rp and rp["runout_difference_m"], "{:+.0f}")} m'),
                    ("Travel angle", f'{fmt(r["runout_angle_deg"], "{:.1f}")}° (com1DFA {fmt(r["runout_angle_com1DFA_deg"], "{:.1f}")}°)'),
                    ("Runout area recall / precision", f'{fmt(r["recall"], "{:.2f}")} / {fmt(r["precision"], "{:.2f}")}'),
                    ("Footprint", f'{fmt(r["footprint_ha"], "{:.1f}")} ha'),
                    ("Max speed", f'{fmt(r["max_pfv_m_s"])} m/s'),
                    ("Simulated / wall / GPU", f'{fmt(r["simulated_time_s"])} / {fmt(r["wall_time_s"])} / {fmt(r["gpu_time_s"])} s'),
                ]
                if params.get("energy_line_mu_eff") not in (None, "nan"):
                    stats.append(("Energy line μ_eff", fmt(params.get("energy_line_mu_eff"), "{:.2f}")))
                figs = [f'<figure><img loading="lazy" src="{plot}" alt="{e(ds)} {e(NAMES[label])}: footprint against com1DFA">'
                        '<figcaption>Footprint against com1DFA (black outline)</figcaption></figure>']
                if "sandbox" in shots:
                    figs.append(f'<figure><img loading="lazy" src="{shots["sandbox"]}" alt="{e(ds)} {e(NAMES[label])}: app sandbox view">'
                                '<figcaption>App: 2D sandbox view, MPM peak thickness</figcaption></figure>')
                if "map" in shots:
                    figs.append(f'<figure class="wide"><img loading="lazy" src="{shots["map"]}" alt="{e(ds)} {e(NAMES[label])}: map overlay">'
                                '<figcaption>App: map overlay on weBIGeo\'s terrain</figcaption></figure>')
                out.append(f'<article class="card"><h4>{e(NAMES[label])}</h4><dl>'
                           + "".join(f"<div><dt>{e(k)}</dt><dd>{e(v)}</dd></div>" for k, v in stats)
                           + f'</dl><div class="figs">{"".join(figs)}</div></article>')
            out.append("</div>")
        out.append("</section>")

    out.append(LIMITS_HTML)
    (OUT / "index.html").write_text("\n".join(out))
    print(f"wrote {OUT / 'index.html'}, {len(list((OUT / 'images').glob('*')))} images")


PAGE_HEAD = """<title>AvaFrame Comparison Report</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,500;9..144,650&family=Source+Sans+3:wght@400;600&family=JetBrains+Mono:wght@400;600&display=swap">
<style>
/* Layout: a single reading column for the text, wide card grids for the runs. Palette: glacier slate with a cold blue accent. */
:root {
  --bg: #f4f6f8; --surface: #ffffff; --fg: #18212b; --muted: #5a6876; --line: #d6dde4; --accent: #1f5f8b; --accent-soft: #e3edf5;
  --font-display: "Fraunces", Georgia, "Times New Roman", serif;
  --font-body: "Source Sans 3", "Helvetica Neue", Arial, sans-serif;
  --font-data: "JetBrains Mono", ui-monospace, Menlo, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #0f151b; --surface: #17202a; --fg: #e3e9ef; --muted: #93a2b1; --line: #2a3642; --accent: #7fb6dd; --accent-soft: #1b2b39; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #0f151b; --surface: #17202a; --fg: #e3e9ef; --muted: #93a2b1; --line: #2a3642; --accent: #7fb6dd; --accent-soft: #1b2b39; color-scheme: dark }
body { background: var(--bg); color: var(--fg); font: 16px/1.55 var(--font-body); padding-inline: 16px; padding-block: 0 48px; }
main, .masthead, .toc, section { max-width: 1240px; margin-inline: auto; }
.masthead { padding-block: 40px 16px; }
.eyebrow { font: 600 12px/1 var(--font-body); letter-spacing: .12em; text-transform: uppercase; color: var(--muted); margin: 0 0 12px; }
h1 { font: 650 clamp(30px, 5vw, 46px)/1.1 var(--font-display); margin: 0 0 12px; text-wrap: balance; }
h2 { font: 600 26px/1.2 var(--font-display); margin: 40px 0 10px; text-wrap: balance; }
h3 { font: 600 17px/1.3 var(--font-body); margin: 26px 0 10px; color: var(--accent); }
h4 { font: 600 16px/1.3 var(--font-body); margin: 0 0 10px; }
p { max-width: 68ch; margin: 0 0 12px; }
.lede { font-size: 18px; color: var(--fg); }
.meta { color: var(--muted); font-size: 14px; }
.toc { position: sticky; top: env(safe-area-inset-top, 0px); z-index: 2; display: flex; flex-wrap: wrap; gap: 6px 16px; padding-block: 10px;
  background: var(--bg); border-bottom: 1px solid var(--line); }
.toc a { color: var(--accent); text-decoration: none; font-weight: 600; font-size: 14px; }
.toc a:hover, .toc a:focus-visible { text-decoration: underline; }
.scroll { overflow-x: auto; }
table.data { border-collapse: collapse; font-size: 14px; min-width: 640px; width: 100%; }
table.data th { text-align: left; font-weight: 600; color: var(--muted); border-bottom: 1px solid var(--line); padding: 6px 10px; }
table.data td { border-bottom: 1px solid var(--line); padding: 6px 10px; font-variant-numeric: tabular-nums; vertical-align: top; }
table.data td .num { font-family: var(--font-data); display: block; }
table.data td .sub { color: var(--muted); font-size: 12px; }
.cards { display: grid; gap: 16px; grid-template-columns: 1fr; }
.card { background: var(--surface); border: 1px solid var(--line); border-radius: 6px; padding: 16px; min-width: 0; }
.card dl { display: grid; grid-template-columns: repeat(auto-fill, minmax(190px, 1fr)); gap: 8px 16px; margin: 0 0 14px; }
.card dt { font-size: 12px; color: var(--muted); letter-spacing: .02em; }
.card dd { margin: 0; font-family: var(--font-data); font-size: 14px; font-variant-numeric: tabular-nums; }
.figs { display: grid; gap: 12px; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); align-items: start; }
.figs figure { margin: 0; min-width: 0; }
.figs figure.wide { grid-column: span 2; }
@media (max-width: 640px) { .figs figure.wide { grid-column: auto; } }
.figs img { display: block; width: 100%; height: auto; border-radius: 4px; border: 1px solid var(--line); background: var(--accent-soft); }
figcaption { font-size: 12px; color: var(--muted); margin-top: 4px; }
ul { max-width: 70ch; padding-left: 20px; }
li { margin-bottom: 6px; }
code { font-family: var(--font-data); font-size: 13px; }
</style>"""

SETUP_HTML = """<section id="setup"><h2>Setup</h2>
<ul>
<li><strong>Test cases:</strong> the five real avalanches of AvaFrame 2.1 with com1DFA null-test benchmarks: Kontertal (avaKot), Gallreide (avaGar), Hintertux (avaHit), Wolfsgruben (avaWog) and Madlein (avaMal).</li>
<li><strong>Sandbox:</strong> the MPM runs on AvaFrame's own 5 m DEM with AvaFrame's release area and thickness, so differences come from the models and not from the terrain. The domain is the whole DEM (512 cells of 5.0–7.7 m), with 65 536 particles. A run stops like com1DFA, when the kinetic energy drops below 1 % of its peak.</li>
<li><strong>Reference:</strong> com1DFA benchmark runs (AvaFrame v1.11, samosAT friction, ρ 200 kg/m³), taken from AvaFrame.</li>
<li><strong>com4FlowPy:</strong> run through AvaFrame 2.1 with its defaults (α 25°, exponent 8). <strong>weBIGeo trajectories:</strong> with the earlier evaluation's parameters (α 25°, deviation 24°, persistence 0.4). Both give a footprint, no flow thickness.</li>
<li><strong>Rating:</strong> AvaFrame's AIMEC (Fischer 2013) with each benchmark's own settings: runout along the avalanche path, travel angle, and recall and precision of the affected area in the runout zone.</li>
</ul>
<h3>Snow materials</h3>
<p>The result depends on the snow material. These are the app's presets; none was tuned to these avalanches, the same way the com1DFA benchmarks use one parameter set for all cases.</p>
<div class="scroll"><table class="data"><thead><tr><th>material model</th><th>preset</th><th>E / ν</th><th>ρ [kg/m³]</th><th>parameters</th><th>Coulomb μ</th></tr></thead><tbody>
<tr><td>Stomakhin 2013</td><td>film snow</td><td>0.14 MPa / 0.2</td><td>400</td><td>ξ 10, θc 2.5·10⁻², θs 7.5·10⁻³</td><td>0.47</td></tr>
<tr><td>Drucker–Prager</td><td>cold dense, cohesionless</td><td>3 MPa / 0.3</td><td>250</td><td>φ 13.3° (from Li's M = 0.5)</td><td>0.47</td></tr>
<tr><td>Cam Clay, Li I</td><td>cold dense</td><td>3 MPa / 0.3</td><td>250</td><td>M 0.5, β 0, ξ 1, p₀ 3 kPa</td><td>0.47</td></tr>
<tr><td>Cam Clay, Li II</td><td>warm shear</td><td>3 MPa / 0.3</td><td>250</td><td>M 1.5, β 0.3, ξ 1, p₀ 30 kPa</td><td>0.47</td></tr>
<tr><td>Cam Clay, Li III</td><td>sliding slab</td><td>3 MPa / 0.3</td><td>250</td><td>M 1.5, β 0.5, ξ 1, p₀ 42 kPa</td><td>0.47</td></tr>
<tr><td>Cam Clay, Li IV</td><td>warm plug</td><td>3 MPa / 0.3</td><td>250</td><td>M 0.5, β 1.0, ξ 0.1, p₀ 12 kPa</td><td>0.47</td></tr>
<tr><td>Cam Clay, Li V</td><td>Vallée de la Sionne 2003</td><td>3 MPa / 0.3</td><td>200</td><td>M 0.7, β 0.2, ξ 0.002, p₀ 3 kPa</td><td>0.49</td></tr>
</tbody></table></div>
<p>Voellmy runs use com1DFA's Voellmy parameters, μ 0.155 and ξ 4000 m/s². Li's cases I–IV describe flow regimes he observed at 0.5 m cells; at 5–7.7 m they carry his parameters, not necessarily his regimes.</p>
<h3>How to read a run</h3>
<ul>
<li><strong>Footprint against com1DFA:</strong> drawn from the exported rasters, the same for every model. Hillshade, release in cyan, the model's peak flow thickness above 0.1 m in viridis (0–5 m), or its footprint in orange for com4FlowPy and the trajectories, and com1DFA's footprint as a black outline.</li>
<li><strong>App screenshots</strong> (MPM runs): the 2D sandbox view at the end of the run, showing the MPM peak thickness with the release and com1DFA outline; and the map with the same result as an overlay on weBIGeo's own terrain. The map terrain is not AvaFrame's DEM: it shows the place, not how the run would go there.</li>
<li><strong>"≥"</strong> marks a runout past the end of the AIMEC path: a lower bound.</li>
</ul></section>"""

LIMITS_HTML = """<section id="limits"><h2>Limits</h2>
<ul>
<li><strong>No samosAT friction in the MPM.</strong> com1DFA's benchmark friction law has no counterpart; Voellmy with com1DFA's own parameters is the closest.</li>
<li><strong>Resolution:</strong> 5–7.7 m cells against Li et al.'s 0.5 m. A 1 m release is a fifth of a cell thick.</li>
<li><strong>com1DFA is a model, not an observation.</strong> Three cases are based on documented events, but no observed runouts are part of the data.</li>
<li><strong>One run per configuration:</strong> the solver is deterministic, and each batch run sets its own time step. There is no study of particle count or cell size yet.</li>
<li><strong>Computing time:</strong> wall time includes rendering. GPU time is the solver's own (Apple M5). com1DFA's benchmark metadata gives only the simulated run time.</li>
</ul>
<p class="meta">Reproduce: <code>mpm-mls-doc/avaframe/</code> in the weBIGeo fork (prepare_dataset.py, batch_full.json, run_com4flowpy.py, compare_aimec.py, summarise.py, report.py). Write-up: <code>mpm-mls-doc/11-avaframe-comparison.md</code>.</p></section>"""


if __name__ == "__main__":
    main()
