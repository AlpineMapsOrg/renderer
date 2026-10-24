"""AvaFrame release areas against the disc the map scenarios used before (one panel per dataset),
for mpm-mls-doc/14-avaframe-release-on-map.md. Reads the prepared datasets (dataset.json and the
release raster); the discs are the map scenarios' from before 2026-10-24 (AvalanchePanel.cpp).

usage (repository root):
  mpm-mls-doc/avaframe/.venv/bin/python -I mpm-mls-doc/avaframe/tools/map_release_figure.py \
      extern/avaframe_sandbox mpm-mls-doc/avaframe-map-release.svg"""
import json, math, sys
import numpy as np

root, out = sys.argv[1], sys.argv[2]
# Scenario discs of the map (AvalanchePanel.cpp before the change): centre lat/lon, radius [m].
DISCS = {
    "avaKot": ("Kontertal", "relKot", (47.25227, 10.39604), 70.0),
    "avaGar": ("Gallreide", "relGar", (47.05046, 11.32931), 114.0),
    "avaHit": ("Hintertux", "relHit", (47.12098, 11.66391), 261.0),
    "avaWog": ("Wolfsgruben", "relWog", (47.12161, 10.28847), 207.0),
    "avaMal": ("Madlein", "relMal1to3", (47.03342, 10.27979), 298.0),
}

def read_asc(path):
    head, rows = {}, []
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 2 and parts[0].isalpha() or (len(parts) == 2 and parts[0].lower() in ("ncols", "nrows", "xllcorner", "yllcorner", "xllcenter", "yllcenter", "cellsize", "nodata_value")):
                head[parts[0].lower()] = float(parts[1])
            elif parts:
                rows.append([float(v) for v in parts])
    return head, np.array(rows)

panels = []
for name, (title, scenario, centre, radius) in DISCS.items():
    meta = json.load(open(f"{root}/{name}/dataset.json"))
    rel = next(s for s in meta["release_scenarios"] if s["name"] == scenario)
    head, grid = read_asc(f"{root}/{name}/{rel['file']}")
    cs = head["cellsize"]
    nrows = grid.shape[0]
    cells = [(c * cs, (nrows - 1 - r) * cs) for r, c in zip(*np.nonzero(grid > 0))]
    # lat/lon -> local metres: affine fit to the DEM corners (SW, SE, NE, NW).
    size = np.array(meta["dem"]["extent_m"])
    local = np.array([[0, 0], [size[0], 0], size, [0, size[1]]], float)
    ll = np.array(meta["location"]["corners_lat_lon"], float)
    A = np.c_[ll, np.ones(4)]
    coef, *_ = np.linalg.lstsq(A, local, rcond=None)
    disc = np.array([*centre, 1.0]) @ coef
    xs = [c[0] for c in cells] + [disc[0] - radius, disc[0] + radius]
    ys = [c[1] for c in cells] + [disc[1] - radius, disc[1] + radius]
    area = rel["features"] and sum(f["area_m2"] for f in rel["features"])
    panels.append((title, scenario, cells, cs, disc, radius, (min(xs), max(xs), min(ys), max(ys)), rel["volume_m3"], area))

W, H, PAD = 300, 345, 28
COLS = 3
rows_n = (len(panels) + COLS - 1) // COLS
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W * COLS}" height="{W * COLS}" font-family="Helvetica, Arial, sans-serif" font-size="12">',
       '<rect width="100%" height="100%" fill="white"/>']
for i, (title, scenario, cells, cs, disc, radius, (x0, x1, y0, y1), volume, area) in enumerate(panels):
    span = max(x1 - x0, y1 - y0) * 1.1
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    s = (W - 2 * PAD) / span
    ox, oy = (i % COLS) * W, (i // COLS) * H
    X = lambda x: ox + PAD + (x - (cx - span / 2)) * s
    Y = lambda y: oy + 32 + (cy + span / 2 - y) * s
    svg.append(f'<text x="{ox + W / 2}" y="{oy + 20}" text-anchor="middle" font-weight="bold">{title} ({scenario})</text>')
    svg.append(f'<rect x="{ox + PAD}" y="{oy + 32}" width="{W - 2 * PAD}" height="{W - 2 * PAD}" fill="#f4f4f4" stroke="#ccc"/>')
    # cells merged into runs along each row
    by_row = {}
    for (x, y) in cells:
        by_row.setdefault(round(y / cs), []).append(round(x / cs))
    for r, xs in by_row.items():
        xs.sort()
        start = prev = xs[0]
        for c in xs[1:] + [None]:
            if c is not None and c == prev + 1:
                prev = c
                continue
            svg.append(f'<rect x="{X(start * cs):.1f}" y="{Y((r + 1) * cs):.1f}" width="{(prev - start + 1) * cs * s + 0.3:.1f}" height="{cs * s + 0.3:.1f}" fill="#2b7bb9"/>')
            if c is not None:
                start = prev = c
    svg.append(f'<circle cx="{X(disc[0]):.1f}" cy="{Y(disc[1]):.1f}" r="{radius * s:.1f}" fill="none" stroke="#e8710a" stroke-width="2" stroke-dasharray="5,3"/>')
    bar = 100 if span < 900 else 500
    by = oy + 32 + W - 2 * PAD + 14
    svg.append(f'<line x1="{ox + PAD}" y1="{by}" x2="{ox + PAD + bar * s:.1f}" y2="{by}" stroke="black" stroke-width="2"/>')
    svg.append(f'<text x="{ox + PAD + bar * s + 6:.1f}" y="{by + 4}">{bar} m</text>')
    svg.append(f'<text x="{ox + PAD}" y="{by + 22}">{area / 1e4:.1f} ha, {volume:,.0f} m³</text>')
# legend in the free sixth cell
lx, ly = (len(panels) % COLS) * W + PAD, (rows_n - 1) * H + 80
svg.append(f'<rect x="{lx}" y="{ly}" width="14" height="14" fill="#2b7bb9"/><text x="{lx + 22}" y="{ly + 12}">AvaFrame release cells,</text><text x="{lx + 22}" y="{ly + 28}">now released on the map</text>')
svg.append(f'<circle cx="{lx + 7}" cy="{ly + 57}" r="7" fill="none" stroke="#e8710a" stroke-width="2" stroke-dasharray="4,2"/><text x="{lx + 22}" y="{ly + 61}">disc of the same area, before</text><text x="{lx + 22}" y="{ly + 77}">(masked by 30–45° slopes)</text>')
svg.append(f'<text x="{lx}" y="{ly + 115}" fill="#555">North up, metres on AvaFrame&apos;s grid.</text>')
svg.append('</svg>')
open(out, "w").write(chr(10).join(svg))
print("wrote", out, [(p[0], len(p[2])) for p in panels])
