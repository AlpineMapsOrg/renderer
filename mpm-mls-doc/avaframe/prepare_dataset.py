#!/usr/bin/env python3
"""Prepare AvaFrame test cases for the MPM sandbox (AvaFrame comparison, phase 1).

For each AvaFrame dataset this writes, into extern/avaframe_sandbox/<name>/:

    dataset.json                 metadata: location, grid, release scenarios, references, path
    dem.asc                      the DEM, normalised (xllcorner form, float, NODATA -9999)
    release_<scenario>.asc       release thickness [m] per DEM cell, 0 outside (one per REL shapefile)
    reference/<benchmark>/       com1DFA reference results (pft, pfv, ppr .asc) + params.json
    path.json                    AIMEC path (polyline) and split point, grid and lat/lon
    preview_<scenario>.png       check image: hillshade, release (cyan), com1DFA footprint
                                 outline (green), AIMEC path (red)
    overlay/*.png + *_aabb.txt   (real datasets only) com1DFA peak thickness and the release
                                 area reprojected to weBIGeo world coordinates (EPSG:3857), for
                                 the Overlays panel

Inputs come from a sparse clone of AvaFrame (data and benchmarks), by default extern/avaframe,
pinned to a release tag:

    git clone --depth 1 --branch 2.1 --filter=blob:none --sparse \\
        https://github.com/OpenNHM/AvaFrame.git extern/avaframe
    git -C extern/avaframe sparse-checkout set avaframe/data benchmarks

Usage (from the repository root, with the venv in mpm-mls-doc/avaframe):

    mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py avaKot avaGar
    mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py --real
    mpm-mls-doc/avaframe/.venv/bin/python mpm-mls-doc/avaframe/prepare_dataset.py --all

Only ESRI ASCII grids are read (GeoTIFF datasets such as avaAlr are skipped with a note).
Real datasets are in MGI / Austria Lambert (EPSG:31287). Synthetic ones either have no .prj or
ship the same .prj with local coordinates (outside Austria); both get no geolocation and no map
overlays.
"""
import argparse
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
NODATA = -9999.0
REAL_DATASETS = ["avaKot", "avaGar", "avaHit", "avaWog", "avaMal"]


# ---------------------------------------------------------------------------------------------
# ESRI ASCII grid
# ---------------------------------------------------------------------------------------------

class Grid:
    """An ESRI ASCII grid. Row 0 is the northern edge; (xll, yll) is the lower-left corner."""

    def __init__(self, data, xll, yll, cellsize, nodata=NODATA):
        self.data = data  # float64, shape (nrows, ncols), NODATA marked as NaN
        self.xll, self.yll, self.cellsize, self.nodata = xll, yll, cellsize, nodata

    @property
    def nrows(self):
        return self.data.shape[0]

    @property
    def ncols(self):
        return self.data.shape[1]

    def cell_centres(self):
        """x, y of every cell centre, shape (nrows, ncols) each."""
        x = self.xll + (np.arange(self.ncols) + 0.5) * self.cellsize
        y = self.yll + (self.nrows - np.arange(self.nrows) - 0.5) * self.cellsize
        return np.meshgrid(x, y)

    def extent(self):
        return (self.xll, self.yll, self.xll + self.ncols * self.cellsize, self.yll + self.nrows * self.cellsize)

    def same_grid(self, other):
        return (self.data.shape == other.data.shape and abs(self.xll - other.xll) < 1e-3 * self.cellsize
                and abs(self.yll - other.yll) < 1e-3 * self.cellsize and abs(self.cellsize - other.cellsize) < 1e-6)


def read_asc(path):
    header = {}
    with open(path) as f:
        for _ in range(6):
            pos = f.tell()
            line = f.readline().split()
            if len(line) != 2 or not line[0][0].isalpha():
                f.seek(pos)
                break
            header[line[0].lower()] = float(line[1])
        data = np.loadtxt(f, dtype=np.float64, ndmin=2)
    ncols, nrows, cs = int(header["ncols"]), int(header["nrows"]), header["cellsize"]
    if data.shape != (nrows, ncols):
        raise ValueError(f"{path}: data {data.shape} does not match header {nrows}x{ncols}")
    # xllcenter / yllcenter refer to the centre of the lower-left cell; normalise to corners.
    xll = header["xllcorner"] if "xllcorner" in header else header["xllcenter"] - cs / 2
    yll = header["yllcorner"] if "yllcorner" in header else header["yllcenter"] - cs / 2
    nodata = header.get("nodata_value", NODATA)
    data[data == nodata] = np.nan
    return Grid(data, xll, yll, cs, NODATA)


def resample_nearest(src, dst):
    """Values of grid src at the cell centres of grid dst (nearest cell, NaN outside)."""
    xc, yc = dst.cell_centres()
    col = np.floor((xc - src.xll) / src.cellsize).astype(int)
    row = np.floor((src.yll + src.nrows * src.cellsize - yc) / src.cellsize).astype(int)
    valid = (col >= 0) & (col < src.ncols) & (row >= 0) & (row < src.nrows)
    out = np.full(xc.shape, np.nan)
    out[valid] = src.data[row[valid], col[valid]]
    return out


def slope_cos(grid):
    """cos of the terrain slope per cell (1 on flat ground)."""
    dzdy, dzdx = np.gradient(grid.data, grid.cellsize)
    return 1.0 / np.sqrt(1.0 + dzdx ** 2 + dzdy ** 2)


def write_asc(path, grid, data=None, fmt="%.3f"):
    data = grid.data if data is None else data
    out = np.where(np.isnan(data), NODATA, data)
    with open(path, "w") as f:
        f.write(f"ncols {grid.ncols}\nnrows {grid.nrows}\nxllcorner {grid.xll:.6f}\nyllcorner {grid.yll:.6f}\n"
                f"cellsize {grid.cellsize:.9f}\nNODATA_value {NODATA:.0f}\n")
        np.savetxt(f, out, fmt=fmt)


# ---------------------------------------------------------------------------------------------
# Shapefiles (only what AvaFrame uses: point 1, polyline 3, polygon 5) and their dBASE tables
# ---------------------------------------------------------------------------------------------

def read_shp(path):
    """Returns a list of shapes; each shape is a list of parts, each part an (n, 2) array."""
    b = Path(path).read_bytes()
    shape_type = struct.unpack("<i", b[32:36])[0]
    shapes, off = [], 100
    while off < len(b):
        content_len = struct.unpack(">i", b[off + 4:off + 8])[0] * 2
        rec = b[off + 8:off + 8 + content_len]
        off += 8 + content_len
        rtype = struct.unpack("<i", rec[:4])[0]
        if rtype == 0:
            shapes.append([])
        elif rtype == 1:
            shapes.append([np.array([struct.unpack("<2d", rec[4:20])])])
        elif rtype in (3, 5):
            nparts, npoints = struct.unpack("<2i", rec[36:44])
            parts = list(struct.unpack(f"<{nparts}i", rec[44:44 + 4 * nparts])) + [npoints]
            pts = np.frombuffer(rec[44 + 4 * nparts:44 + 4 * nparts + 16 * npoints], dtype="<f8").reshape(-1, 2)
            shapes.append([pts[parts[i]:parts[i + 1]] for i in range(nparts)])
        else:
            raise ValueError(f"{path}: unsupported shape type {rtype}")
    return shape_type, shapes


def read_dbf(path):
    b = Path(path).read_bytes()
    n = struct.unpack("<I", b[4:8])[0]
    header_len, record_len = struct.unpack("<HH", b[8:12])
    fields, i = [], 32
    while b[i] != 0x0D:
        fields.append((b[i:i + 11].split(b"\0")[0].decode(), b[i + 16]))
        i += 32
    records = []
    for r in range(n):
        off = header_len + r * record_len + 1
        rec = {}
        for name, length in fields:
            rec[name] = b[off:off + length].decode("latin1").strip()
            off += length
        records.append(rec)
    return records


def rasterise_polygons(grid, shapes):
    """Boolean mask of the cells whose centre lies inside the shapes (even-odd rule, so holes
    are holes). Per shape, so each feature can carry its own thickness."""
    xc, yc = grid.cell_centres()
    masks = []
    for parts in shapes:
        inside = np.zeros(xc.shape, dtype=bool)
        for ring in parts:
            x0, y0 = ring[:-1, 0], ring[:-1, 1]
            x1, y1 = ring[1:, 0], ring[1:, 1]
            for ax, ay, bx, by in zip(x0, y0, x1, y1):
                if ay == by:
                    continue
                crosses = (ay > yc) != (by > yc)
                x_at = ax + (yc - ay) * (bx - ax) / (by - ay)
                inside ^= crosses & (xc < x_at)
        masks.append(inside)
    return masks


# ---------------------------------------------------------------------------------------------
# PNG output (RGBA, no imaging library needed)
# ---------------------------------------------------------------------------------------------

def write_png(path, rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\0" + rgba[y].astype(np.uint8).tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def hillshade(grid, azimuth=315.0, altitude=45.0):
    z = np.nan_to_num(grid.data, nan=np.nanmin(grid.data))
    dzdy, dzdx = np.gradient(z, grid.cellsize)  # rows go south, so dzdy is -d/dy
    slope = np.arctan(np.hypot(dzdx, dzdy))
    aspect = np.arctan2(-dzdy, -dzdx)  # downslope direction, x east and y north
    az, alt = math.radians(90.0 - azimuth), math.radians(altitude)
    shade = np.sin(alt) * np.cos(slope) + np.cos(alt) * np.sin(slope) * np.cos(az - aspect)
    shade = np.clip(shade, 0, 1)
    shade[np.isnan(grid.data)] = np.nan
    return shade


def colormap(values, vmax):
    """Peak values to RGBA: dark blue -> cyan -> yellow -> red, transparent at 0 / NaN."""
    stops = np.array([[30, 40, 140], [0, 180, 220], [250, 230, 60], [220, 40, 30]], dtype=float)
    t = np.clip(np.nan_to_num(values) / vmax, 0, 1) * (len(stops) - 1)
    i = np.minimum(t.astype(int), len(stops) - 2)
    f = (t - i)[..., None]
    rgb = stops[i] * (1 - f) + stops[i + 1] * f
    alpha = np.where(np.nan_to_num(values) > 0, 220, 0)[..., None]
    return np.concatenate([rgb, alpha], axis=-1)


def outline(mask):
    """Cells of the mask that touch a cell outside it."""
    m = np.pad(mask, 1)
    interior = m[1:-1, 1:-1] & m[:-2, 1:-1] & m[2:, 1:-1] & m[1:-1, :-2] & m[1:-1, 2:]
    return mask & ~interior


def draw_polyline(rgba, grid, pts, color, width=1):
    h, w = rgba.shape[:2]
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        n = int(max(abs(bx - ax), abs(by - ay)) / grid.cellsize * 2) + 2
        for t in np.linspace(0, 1, n):
            col = int(((ax + t * (bx - ax)) - grid.xll) / grid.cellsize)
            row = int((grid.yll + grid.nrows * grid.cellsize - (ay + t * (by - ay))) / grid.cellsize)
            rgba[max(row - width, 0):min(row + width + 1, h), max(col - width, 0):min(col + width + 1, w)] = color


# ---------------------------------------------------------------------------------------------
# Georeferencing: EPSG:31287 -> WGS84 and weBIGeo world coordinates (EPSG:3857)
# ---------------------------------------------------------------------------------------------

def transformers(crs_wkt):
    from pyproj import CRS
    from pyproj.transformer import TransformerGroup

    src = CRS.from_wkt(crs_wkt)
    # Several MGI -> WGS84 transformations exist; take the most accurate one available.
    def best(dst):
        group = TransformerGroup(src, dst, always_xy=True)
        ok = [t for t in group.transformers if t.accuracy and t.accuracy > 0] or group.transformers
        return min(ok, key=lambda t: t.accuracy if t.accuracy > 0 else 1e9)

    to_lonlat, to_world = best(4326), best(3857)
    to_src = TransformerGroup(3857, src, always_xy=True).transformers[0]
    return to_lonlat, to_world, to_src


def in_area_of_use(crs_wkt, lon, lat):
    from pyproj import CRS

    crs = CRS.from_wkt(crs_wkt)
    area = crs.area_of_use or CRS.from_epsg(crs.to_epsg(min_confidence=20) or 31287).area_of_use
    return area.west <= lon <= area.east and area.south <= lat <= area.north


def reproject_to_world(grid, values, to_world, to_src, nearest=True):
    """Resamples a raster on the DEM grid to an axis-aligned EPSG:3857 raster covering it.
    Returns (image values, (x_min, y_min, x_max, y_max) in world coordinates)."""
    x0, y0, x1, y1 = grid.extent()
    cx, cy = np.array([x0, x1, x1, x0]), np.array([y0, y0, y1, y1])
    wx, wy = to_world.transform(cx, cy)
    wx0, wx1, wy0, wy1 = wx.min(), wx.max(), wy.min(), wy.max()
    # Keep the source resolution: one Mercator unit is cos(latitude) metres.
    lat = math.degrees(math.atan(math.sinh((wy0 + wy1) / 2 / 6378137.0)))
    step = grid.cellsize / math.cos(math.radians(lat))
    w, h = int(math.ceil((wx1 - wx0) / step)), int(math.ceil((wy1 - wy0) / step))
    px = wx0 + (np.arange(w) + 0.5) * step
    py = wy1 - (np.arange(h) + 0.5) * step
    gx, gy = np.meshgrid(px, py)
    sx, sy = to_src.transform(gx.ravel(), gy.ravel())
    col = np.floor((sx - grid.xll) / grid.cellsize).astype(int)
    row = np.floor((grid.yll + grid.nrows * grid.cellsize - sy) / grid.cellsize).astype(int)
    valid = (col >= 0) & (col < grid.ncols) & (row >= 0) & (row < grid.nrows)
    out = np.full(gx.size, np.nan)
    out[valid] = values[row[valid], col[valid]]
    return out.reshape(h, w), (wx0, wy1 - h * step, wx0 + w * step, wy1)


def write_aabb(path, aabb):
    Path(path).write_text("\n".join(f"{v:.3f}" for v in aabb) + "\n")


# ---------------------------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------------------------

def find_benchmarks(avaframe, name):
    """com1DFA null-test benchmarks of this dataset, keyed by release scenario."""
    found = []
    for d in sorted((avaframe / "benchmarks").iterdir()):
        des = d / f"{d.name}_desDict.json"
        if not des.exists():
            continue
        info = json.loads(des.read_text())
        if info.get("AVANAME") != name or info.get("simType") != "null":
            continue
        params = info.get("Simulation Parameters", {})
        files = {}
        for kind in ("pft", "pfv", "ppr"):
            hits = [p for p in d.iterdir() if p.name.endswith(f"_{kind}.asc") or p.name.endswith(f"_{kind}.tif")]
            if hits:
                files[kind] = hits[0]
        found.append({"benchmark": d.name, "dir": d, "release_scenario": params.get("Release Area Scenario"),
                      "params": params, "files": files})
    return found


def prepare(avaframe, out_root, name, version):
    inputs = avaframe / "avaframe" / "data" / name / "Inputs"
    dems = sorted(inputs.glob("*.asc"))
    if not dems:
        tifs = sorted(inputs.glob("*.tif"))
        print(f"  {name}: skipped - DEM is {'GeoTIFF' if tifs else 'missing'} (only .asc is supported)")
        return None
    out = out_root / name
    if out.exists():
        shutil.rmtree(out)
    (out / "reference").mkdir(parents=True)

    dem = read_asc(dems[0])
    write_asc(out / "dem.asc", dem)
    rel_shps = sorted((inputs / "REL").glob("*.shp"))
    prj = next(iter(sorted((inputs / "REL").glob("*.prj"))), None)
    crs_wkt = prj.read_text() if prj else None
    to_lonlat = to_world = to_src = None
    local_note = None
    if crs_wkt:
        to_lonlat, to_world, to_src = transformers(crs_wkt)
        # Synthetic DEMs ship the same Austria Lambert .prj as the real ones, but their coordinates
        # are local (origin near 0, 0), which would put them in the sea off Liguria. Only geolocate
        # a DEM whose centre lies inside the CRS's area of use (Austria).
        x0, y0, x1, y1 = dem.extent()
        lon_c, lat_c = to_lonlat.transform((x0 + x1) / 2, (y0 + y1) / 2)
        if not in_area_of_use(crs_wkt, lon_c, lat_c):
            to_lonlat = to_world = to_src = None
            local_note = "synthetic: .prj is Austria Lambert, but the coordinates are local (outside Austria)"

    meta = {
        "name": name,
        "source": {"avaframe_version": version["tag"], "avaframe_commit": version["commit"],
                   "inputs": str(inputs.relative_to(avaframe))},
        "crs": "EPSG:31287 (MGI / Austria Lambert)" if to_lonlat else None,
        "crs_note": local_note,
        "dem": {"file": "dem.asc", "source": dems[0].name, "ncols": dem.ncols, "nrows": dem.nrows,
                "cellsize": dem.cellsize, "xllcorner": dem.xll, "yllcorner": dem.yll,
                "extent_m": [round(dem.ncols * dem.cellsize, 1), round(dem.nrows * dem.cellsize, 1)],
                "altitude_m": [round(float(np.nanmin(dem.data)), 1), round(float(np.nanmax(dem.data)), 1)],
                "nodata_cells": int(np.isnan(dem.data).sum())},
        "release_scenarios": [], "references": [],
    }
    if to_lonlat:
        x0, y0, x1, y1 = dem.extent()
        lon, lat = to_lonlat.transform(np.array([x0, x1, x1, x0, (x0 + x1) / 2]), np.array([y0, y0, y1, y1, (y0 + y1) / 2]))
        meta["location"] = {"centre_lat_lon": [round(float(lat[4]), 6), round(float(lon[4]), 6)],
                            "corners_lat_lon": [[round(float(a), 6), round(float(b), 6)] for a, b in zip(lat[:4], lon[:4])],
                            "transformation": to_lonlat.description, "accuracy_m": to_lonlat.accuracy}

    # Release scenarios: one shapefile each; every feature has its own thickness.
    shade = hillshade(dem)
    footprint_by_scenario = {}
    for shp in rel_shps:
        scenario = shp.stem
        _, shapes = read_shp(shp)
        records = read_dbf(shp.with_suffix(".dbf"))
        thickness = np.zeros(dem.data.shape)
        features = []
        for mask, rec in zip(rasterise_polygons(dem, shapes), records):
            th = float(rec.get("thickness") or 1.0)
            thickness[mask] = th
            features.append({"name": rec.get("Name") or rec.get("name"), "thickness_m": th, "cells": int(mask.sum()),
                             "area_m2": round(float(mask.sum()) * dem.cellsize ** 2, 1),
                             "avalanche": rec.get("Name_EZG"), "basis": rec.get("Comment"),
                             "thickness_from": "shapefile" if rec.get("thickness") else "default 1.0 m"})
        thickness[np.isnan(dem.data)] = np.nan
        write_asc(out / f"release_{scenario}.asc", dem, thickness, fmt="%.3f")
        # AvaFrame's release thickness is measured perpendicular to the slope and its area on
        # the inclined surface: volume = sum(thickness / cos(slope)) * cell area. Checked on
        # avaKot against com1DFA's initial mass (21 640 m3 at 200 kg/m3): 0.2 % apart, while
        # the projected volume is 30 % low. A vertical column holds thickness / cos(slope).
        released = np.nan_to_num(thickness) > 0
        cos_slope = slope_cos(dem)
        meta["release_scenarios"].append({
            "name": scenario, "file": f"release_{scenario}.asc", "features": features,
            "thickness_convention": "perpendicular to the slope (vertical depth = thickness / cos(slope))",
            "volume_m3": round(float(np.sum(thickness[released] / cos_slope[released])) * dem.cellsize ** 2, 1),
            "volume_projected_m3": round(float(np.nansum(thickness)) * dem.cellsize ** 2, 1),
            "mean_slope_deg": round(float(np.degrees(np.arccos(cos_slope[released])).mean()), 1) if released.any() else None})
        footprint_by_scenario[scenario] = np.nan_to_num(thickness) > 0

    # References: com1DFA null-test benchmarks, matched to their release scenario.
    for ref in find_benchmarks(avaframe, name):
        rdir = out / "reference" / ref["benchmark"]
        rdir.mkdir(parents=True)
        entry = {"benchmark": ref["benchmark"], "release_scenario": ref["release_scenario"], "files": {},
                 "friction_model": ref["params"].get("Friction model"), "density": ref["params"].get("Density [kgm-3]"),
                 "program_version": ref["params"].get("Program version"),
                 "run_time_s": ref["params"].get("Avalanche run time [s]")}
        for kind, src in ref["files"].items():
            if src.suffix != ".asc":
                entry["files"][kind] = f"skipped ({src.suffix})"
                continue
            grid = read_asc(src)
            # com1DFA remeshes the DEM to its own cell size (5 m), so its results are usually
            # on a slightly different grid than the DEM; they are kept on theirs.
            entry["grid"] = ("same as the DEM" if grid.same_grid(dem) else
                             {"ncols": grid.ncols, "nrows": grid.nrows, "cellsize": grid.cellsize,
                              "xllcorner": grid.xll, "yllcorner": grid.yll})
            write_asc(rdir / f"{kind}.asc", grid, fmt="%.4f")
            entry["files"][kind] = f"reference/{ref['benchmark']}/{kind}.asc"
            entry[f"max_{kind}"] = round(float(np.nanmax(grid.data)), 3)
            if kind == "pft":
                entry["footprint_cells"] = int((np.nan_to_num(grid.data) > 0).sum())
                entry["footprint_area_m2"] = round(entry["footprint_cells"] * dem.cellsize ** 2, 1)
        (rdir / "params.json").write_text(json.dumps(ref["params"], indent=2, ensure_ascii=False))
        meta["references"].append(entry)

    # AIMEC path and split point.
    path_meta = {}
    for key, pattern in (("path", "path_aimec.shp"), ("split_point", "splitPoint.shp")):
        files = sorted(inputs.glob(f"*/{pattern}"))
        if not files:
            continue
        _, shapes = read_shp(files[0])
        pts = shapes[0][0]
        item = {"xy": [[round(float(x), 3), round(float(y), 3)] for x, y in pts]}
        if to_lonlat:
            lon, lat = to_lonlat.transform(pts[:, 0], pts[:, 1])
            item["lat_lon"] = [[round(float(a), 7), round(float(b), 7)] for a, b in zip(lat, lon)]
        path_meta[key] = item
    if path_meta:
        (out / "path.json").write_text(json.dumps(path_meta, indent=1))
        meta["path"] = "path.json"

    # Check images on the DEM grid: hillshade, release, reference footprint, path.
    for scenario, footprint in footprint_by_scenario.items():
        grey = np.nan_to_num(shade * 200 + 40, nan=255)
        rgba = np.stack([grey, grey, grey, np.full(grey.shape, 255.0)], axis=-1)
        rgba[footprint] = rgba[footprint] * 0.4 + np.array([0, 220, 230, 255]) * 0.6
        rgba[outline(footprint)] = [0, 160, 200, 255]
        refs = [r for r in meta["references"] if r["release_scenario"] == scenario and "footprint_cells" in r]
        if refs:
            grid = read_asc(out / refs[0]["files"]["pft"])
            rgba[outline(np.nan_to_num(resample_nearest(grid, dem)) > 0)] = [40, 200, 40, 255]
        if "path" in path_meta:
            draw_polyline(rgba, dem, np.array(path_meta["path"]["xy"]), [220, 40, 40, 255])
        write_png(out / f"preview_{scenario}.png", rgba)

    # Map overlays (real datasets): com1DFA peak flow thickness and the release area.
    if to_world:
        (out / "overlay").mkdir()
        overlays = []
        for entry in meta["references"]:
            if "footprint_cells" not in entry:
                continue
            grid = read_asc(out / entry["files"]["pft"])
            img, aabb = reproject_to_world(grid, grid.data, to_world, to_src)
            stem = f"com1DFA_{entry['release_scenario']}_pft"
            write_png(out / "overlay" / f"{stem}.png", colormap(img, max(entry["max_pft"], 1e-3)))
            write_aabb(out / "overlay" / f"{stem}_aabb.txt", aabb)
            overlays.append(f"overlay/{stem}.png")
        for scenario, footprint in footprint_by_scenario.items():
            img, aabb = reproject_to_world(dem, footprint.astype(float), to_world, to_src)
            rgba = np.zeros(img.shape + (4,))
            rgba[np.nan_to_num(img) > 0] = [0, 220, 230, 200]
            write_png(out / "overlay" / f"release_{scenario}.png", rgba)
            write_aabb(out / "overlay" / f"release_{scenario}_aabb.txt", aabb)
            overlays.append(f"overlay/release_{scenario}.png")
        meta["map_overlays"] = overlays

    (out / "dataset.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False))
    return meta


def avaframe_version(avaframe):
    def git(*args):
        try:
            return subprocess.run(["git", "-C", str(avaframe), *args], capture_output=True, text=True, check=True).stdout.strip()
        except Exception:
            return None
    return {"tag": git("describe", "--tags", "--exact-match") or git("describe", "--tags"), "commit": git("rev-parse", "HEAD")}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("datasets", nargs="*", help="dataset names, e.g. avaKot")
    parser.add_argument("--real", action="store_true", help=f"the real datasets: {', '.join(REAL_DATASETS)}")
    parser.add_argument("--all", action="store_true", help="every dataset with an .asc DEM")
    parser.add_argument("--avaframe", default=str(REPO / "extern" / "avaframe"), help="AvaFrame checkout")
    parser.add_argument("--out", default=str(REPO / "extern" / "avaframe_sandbox"), help="output folder")
    args = parser.parse_args()

    avaframe, out_root = Path(args.avaframe), Path(args.out)
    if not (avaframe / "avaframe" / "data").exists():
        sys.exit(f"No AvaFrame data in {avaframe} - see the clone command at the top of this file.")
    names = list(args.datasets)
    if args.real:
        names += REAL_DATASETS
    if args.all:
        names += sorted(p.name for p in (avaframe / "avaframe" / "data").iterdir() if (p / "Inputs").exists())
    if not names:
        parser.error("name at least one dataset, or use --real / --all")

    version = avaframe_version(avaframe)
    out_root.mkdir(parents=True, exist_ok=True)
    summary = []
    for name in dict.fromkeys(names):
        print(f"{name}:")
        meta = prepare(avaframe, out_root, name, version)
        if not meta:
            continue
        loc = meta.get("location", {}).get("centre_lat_lon")
        rel = ", ".join(f"{s['name']} {s['volume_m3']:.0f} m3 (slope {s['mean_slope_deg']} deg)" for s in meta["release_scenarios"])
        refs = ", ".join(f"{r['benchmark']} ({r.get('footprint_area_m2', 0) / 1e4:.1f} ha)" for r in meta["references"]) or "none"
        print(f"  DEM {meta['dem']['ncols']}x{meta['dem']['nrows']} @ {meta['dem']['cellsize']:.2f} m, "
              f"{'lat/lon ' + str(loc) if loc else 'no geolocation (synthetic)'}")
        print(f"  release: {rel}\n  references: {refs}")
        summary.append({"name": name, "location": loc, "release_scenarios": [s["name"] for s in meta["release_scenarios"]],
                        "references": [r["benchmark"] for r in meta["references"]]})
    (out_root / "index.json").write_text(json.dumps({"avaframe": version, "datasets": summary}, indent=2))


if __name__ == "__main__":
    main()
