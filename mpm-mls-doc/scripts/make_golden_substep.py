#!/usr/bin/env python3
"""Golden values for the whole-kernel substep test (mpm-mls-doc/bench/check_substep.mjs).

The GPU golden tests (make_golden.py) check the shader *functions*. This one checks the
*kernels* working together: the particle sort, P2G with its workgroup accumulation and fixed-point
atomics, the grid update with the terrain collision, and G2P with its plasticity and terrain
contact, all on the GPU with the real bindings. The input is 300 particles on a planar slope in
a known state (F, C, tau, velocity), placed so that they share grid nodes (the scatter sums and the
workgroup window are exercised) and stencil nodes lie below the terrain (grid collision, Coulomb
friction). Two scenarios:
  free     no particle touches the surface; three substeps
  contact  60 particles move into the surface within the first substep (particle-level contact);
           one substep. A particle that touched the surface stays within float32 noise of it
           (gravity presses it into the slope by ~1e-3 m per substep), so its contact decision in
           the next substep is a coin flip on the GPU and differs from float64 by ~0.02 m/s: contact
           is therefore compared after one substep only.
This file advances the same particles in float64, mirroring the shaders step by step including the
fixed-point rounding of the grid accumulators, and writes the particle buffers (struct Particle, 32
floats each) before and after the substeps.

    python3 make_golden_substep.py        # rewrites bench/golden/substep_golden.json
"""
import itertools
import json
import math
import os

import numpy as np

import test_material_ccc as ccc
import test_material_dp as dp
import test_mpm as snow

OUT = os.path.join(os.path.dirname(__file__), "..", "bench", "golden", "substep_golden.json")

# The benchmark's configuration (bench/mpm_bench.js DEFAULTS), region-relative metres.
REGION, DOMAIN, GRID_RES, LAYERS = 8000.0, 4000.0, 320, 16
DX = DOMAIN / GRID_RES
ORIGIN = (REGION - DOMAIN) / 2
DT, GRAVITY, DENSITY, MU = 0.01, 9.81, 400.0, 0.47
MASS_SCALE, MOMENTUM_SCALE, MOMENTUM_LIMIT = 1048576.0, 10000.0, 2.0e9
PARTICLE_VOLUME = 1.0 / DENSITY
N_PARTICLES = 300
# scenario: (a few particles start just above the surface, moving into it; substeps to compare)
SCENARIOS = {"free": (False, (1, 3)), "contact": (True, (1,))}

# Planar terrain h = H0 + SX (x - 4000) + SY (y - 4000): the bilinear height texture reproduces it
# exactly, and so do the central-difference normals.
SX, SY = 0.35, 0.15
CENTRE = np.array([3500.0, 3500.0])
NORMAL = np.array([-SX, -SY, 1.0]) / np.linalg.norm([-SX, -SY, 1.0])


def height(x, y, h0):
    return h0 + SX * (x - 4000.0) + SY * (y - 4000.0)


def choose_h0():
    """An offset for which no decision of the kernels (the column floor, node below the surface)
    is within 2 mm of a tie, so float32 and float64 cannot disagree on it."""
    for h0 in np.arange(2000.0, 2001.0, 0.037):
        ok = True
        for i, j in itertools.product(range(int((3500 - 150 - ORIGIN) / DX), int((3500 + 150 - ORIGIN) / DX) + 2), repeat=2):
            h = height(ORIGIN + i * DX, ORIGIN + j * DX, h0)
            frac = (h / DX) % 1.0
            if min(frac, 1.0 - frac) * DX < 2e-3:
                ok = False
                break
            # nodes of this column against the surface
            floor_k = math.floor(h / DX) - 2
            for k in range(floor_k, floor_k + LAYERS):
                if abs(k * DX - h) < 2e-3:
                    ok = False
        if ok:
            return float(h0)
    raise RuntimeError("no tie-free terrain offset found")


H0 = choose_h0()


def floor_col(i, j):
    return math.floor(height(ORIGIN + i * DX, ORIGIN + j * DX, H0) / DX) - 2


STATS = {"grid_collisions": 0, "particle_contacts": 0}
# A particle that ends a substep within float32 error of the surface may take the other branch of
# the contact decision on the GPU, and its velocity then differs by (1 + mu) |v.n|, up to metres per
# second. MARGINS["surface"] is the smallest distance to the surface of any particle at the end of a
# substep over a run; the generator rejects inputs below MIN_SURFACE_MARGIN, about two float32
# spacings at 2000-4000 m. The other branches of the contact law are continuous (at v.n = 0 and at
# the stick/slip boundary both sides give the same velocity).
MARGINS = {"surface": math.inf}
MIN_SURFACE_MARGIN = 4e-4


def collide(v, n=NORMAL):
    """coulomb_friction of mpm_friction.wgsl through resolve_terrain_collision."""
    vn = float(np.dot(v, n))
    if vn >= 0.0:
        return v
    vt = v - n * vn
    vt_len = np.linalg.norm(vt)
    if vt_len <= -MU * vn:
        return np.zeros(3)
    return vt * (1.0 + MU * vn / vt_len)


# --- materials: (plasticity(F_trial, state) -> F, state, tau), initial state ---------------

def materials():
    E_s, nu_s = 1.4e5, 0.2
    E, nu = 3.0e6, 0.3
    lam_mu = lambda E_, nu_: (E_ * nu_ / ((1 + nu_) * (1 - 2 * nu_)), E_ / (2 * (1 + nu_)))

    def stomakhin(F, state):
        F_new, state_new = snow.apply_plasticity(F, state)
        return F_new, state_new, snow.snow_stress(F_new, state_new)

    def drucker_prager(F, state):
        F_new, state_new, _ = dp.dp_plasticity(F, state)
        return F_new, state_new, dp.dp_stress(F_new)

    def cam_clay(F, state):
        F_new, state_new, _, _ = ccc.ccc_plasticity(F, state)
        return F_new, state_new, ccc.ccc_stress(F_new)

    lam, mu = lam_mu(E, nu)
    sin_phi = math.sin(math.radians(13.3))
    dp.settings.mu_0, dp.settings.lambda_0 = mu, lam
    dp.settings.dp_alpha = math.sqrt(2.0 / 3.0) * 2.0 * sin_phi / (3.0 - sin_phi)
    ccc.settings = ccc.Settings(E=E, nu=nu, M=0.7, beta=0.2, xi=0.002, p0=3000.0)
    assert abs(snow.MU_0 - lam_mu(E_s, nu_s)[1]) < 1e-6 and snow.HARDENING == 10.0
    return {
        "stomakhin": (stomakhin, 1.0),
        "dp": (drucker_prager, 0.0),
        "ccc": (cam_clay, ccc.ccc_initial_state()),
    }


# --- the substep, mirroring mpm_p2g / mpm_grid_update / mpm_g2p -----------------------------

def kernel(position):
    gp = np.array([(position[0] - ORIGIN) / DX, (position[1] - ORIGIN) / DX, position[2] / DX])
    base = np.floor(gp - 0.5).astype(int)
    fx = gp - base
    w = [0.5 * (1.5 - fx) ** 2, 0.75 - (fx - 1.0) ** 2, 0.5 * (fx - 0.5) ** 2]
    return base, fx, w


def in_band(node):
    return 0 <= node[0] < GRID_RES and 0 <= node[1] < GRID_RES and 0 <= node[2] - floor_col(node[0], node[1]) < LAYERS


def substep(S, plasticity):
    n = len(S["x"])
    mass_units, momentum = {}, {}
    for p in range(n):  # P2G
        base, fx, w = kernel(S["x"][p])
        affine = -DT * PARTICLE_VOLUME * (4.0 / DX**2) * S["tau"][p] + 1.0 * S["C"][p]
        for i, j, l in itertools.product(range(3), repeat=3):
            node = (int(base[0] + i), int(base[1] + j), int(base[2] + l))
            if not in_band(node):
                continue
            weight = w[i][0] * w[j][1] * w[l][2]
            dpos = (np.array([i, j, l], dtype=float) - fx) * DX
            m = weight * (S["v"][p] + affine @ dpos)
            mass_units[node] = mass_units.get(node, 0) + int(np.round(weight * MASS_SCALE))
            fixed = np.clip(np.round(m * MOMENTUM_SCALE), -MOMENTUM_LIMIT, MOMENTUM_LIMIT).astype(np.int64)
            momentum[node] = momentum.get(node, np.zeros(3, dtype=np.int64)) + fixed

    velocity = {}
    for node, units in mass_units.items():  # grid update
        mass = units / MASS_SCALE
        if mass <= 0.0:
            velocity[node] = np.zeros(3)
            continue
        v = momentum[node] / MOMENTUM_SCALE / mass
        v[2] -= GRAVITY * DT
        world = np.array([ORIGIN + node[0] * DX, ORIGIN + node[1] * DX, node[2] * DX])
        if world[2] < height(world[0], world[1], H0):
            STATS["grid_collisions"] += 1
            v = collide(v)
        assert 2 <= node[0] < GRID_RES - 3 and 2 <= node[1] < GRID_RES - 3, "domain walls are not part of this test"
        velocity[node] = v

    new = {k: S[k].copy() for k in S}
    for p in range(n):  # G2P
        base, fx, w = kernel(S["x"][p])
        v_new, c_new = np.zeros(3), np.zeros((3, 3))
        for i, j, l in itertools.product(range(3), repeat=3):
            node = (int(base[0] + i), int(base[1] + j), int(base[2] + l))
            if not in_band(node):
                continue
            node_v = velocity.get(node, np.zeros(3))
            weight = w[i][0] * w[j][1] * w[l][2]
            dpos = np.array([i, j, l], dtype=float) - fx
            v_new += weight * node_v
            c_new += 4.0 / DX * weight * np.outer(node_v, dpos)
        F_new, state_new, tau_new = plasticity((np.eye(3) + DT * c_new) @ S["F"][p], S["state"][p])
        x_new = S["x"][p] + DT * v_new
        surface = height(x_new[0], x_new[1], H0)
        MARGINS["surface"] = min(MARGINS["surface"], abs(x_new[2] - surface))
        if x_new[2] < surface:
            STATS["particle_contacts"] += 1
            x_new[2] = surface
            v_new = collide(v_new)
        new["x"][p], new["v"][p], new["C"][p], new["F"][p], new["tau"][p], new["state"][p] = x_new, v_new, c_new, F_new, tau_new, state_new
    return new


# --- particle buffer: struct Particle, 32 floats ---------------------------------------------

def pack(S):
    rows = []
    for p in range(len(S["x"])):
        C, F, tau = S["C"][p], S["F"][p], 0.5 * (S["tau"][p] + S["tau"][p].T)
        rows.append([*S["x"][p], 1.0, *S["v"][p], S["state"][p],
                     *C[0], tau[0, 0], *C[1], tau[1, 1], *C[2], tau[2, 2],
                     *F[0], tau[0, 1], *F[1], tau[0, 2], *F[2], tau[1, 2]])
    return [float(np.float32(x)) for row in rows for x in row]


def make_particles(rng, plasticity, initial_state, contact):
    n = N_PARTICLES
    x = np.zeros((n, 3))
    v = np.zeros((n, 3))
    for p in range(n):
        xy = CENTRE + rng.uniform(-50.0, 50.0, 2)
        kind = p % 5
        if kind < 3:      # within the stencil's reach of the surface: nodes below the terrain
            d, vz = rng.uniform(2.0, 20.0), rng.uniform(-6.0, 2.0)
        elif kind == 3 and contact:   # a few centimetres above the surface, moving into it
            d, vz = rng.uniform(0.002, 0.03), rng.uniform(-6.0, -3.5)   # travel 35-60 mm: all cross
        elif kind == 3:               # close to the surface, but staying above it for three substeps
            d, vz = rng.uniform(0.5, 3.0), rng.uniform(-6.0, -3.0)
        else:             # free flight
            d, vz = rng.uniform(20.0, 60.0), rng.uniform(-3.0, 3.0)
        x[p] = [xy[0], xy[1], height(xy[0], xy[1], H0) + d]
        v[p] = [rng.uniform(-8.0, 8.0), rng.uniform(-8.0, 8.0), vz]
    C = rng.uniform(-0.15, 0.15, (n, 3, 3))
    F, tau, state = np.zeros((n, 3, 3)), np.zeros((n, 3, 3)), np.zeros(n)
    for p in range(n):  # an admissible state: the return mapping of a random deformation
        F[p], state[p], tau[p] = plasticity(np.eye(3) + 0.03 * rng.normal(size=(3, 3)), initial_state)
    return {"x": x, "v": v, "C": C, "F": F, "tau": tau, "state": state}


def generate(seed, contact, substeps):
    """All materials for one seed: the golden entries, the smallest surface margin and the contact counts."""
    entries, stats = [], {}
    for name, (plasticity, initial_state) in materials().items():
        MARGINS.update({k: math.inf for k in MARGINS})
        STATS.update(grid_collisions=0, particle_contacts=0)
        S = make_particles(np.random.default_rng(seed), plasticity, initial_state, contact)
        entry = {"material": name, "input": pack(S)}
        step = 0
        for target in substeps:
            while step < target:
                S = substep(S, plasticity)
                step += 1
            entry[f"after{target}"] = pack(S)
        entries.append(entry)
        stats[name] = {**STATS, **MARGINS, "mean_speed": float(np.linalg.norm(S["v"], axis=1).mean())}
    return entries, stats


def main():
    scenarios = []
    for scenario, (contact, substeps) in SCENARIOS.items():
        for seed in range(20261108, 20261108 + 500):
            entries, stats = generate(seed, contact, substeps)
            margin = min(s["surface"] for s in stats.values())
            if margin >= MIN_SURFACE_MARGIN:
                break
        else:
            raise RuntimeError(f"no seed with a wide enough surface margin for {scenario}")
        print(f"{scenario} (seed {seed}, smallest surface margin {margin:.1e} m):")
        for name, s in stats.items():
            print(f"  {name}: {s['grid_collisions']} grid-node collisions and {s['particle_contacts']} particle contacts in "
                  f"{substeps[-1]} substep(s); mean speed {s['mean_speed']:.3f} m/s")
        scenarios.append({"name": scenario, "seed": seed, "substeps": list(substeps), "entries": entries})
    golden = {
        "comment": "Generated by mpm-mls-doc/scripts/make_golden_substep.py; do not edit.",
        "dt": DT, "particles": N_PARTICLES,
        "terrain": {"h0": H0, "sx": SX, "sy": SY},
        "layout": "struct Particle, 32 floats per particle (mpm_common.wgsl)",
        "scenarios": scenarios,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(golden, f)
    print("terrain offset", H0, "->", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
