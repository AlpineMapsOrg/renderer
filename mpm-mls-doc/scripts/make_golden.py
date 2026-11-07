#!/usr/bin/env python3
"""Golden values for the GPU tests of the MPM shader functions (unittests/webgpu_compute).

Runs the Python ports (test_svd, test_mpm, test_material_dp, test_material_ccc, test_friction)
on fixed inputs and writes inputs and outputs to unittests/webgpu_compute/golden/mpm_golden.json.
test_MpmGolden.cpp runs the real WGSL functions on the same inputs and compares.

The ports are float64, the GPU float32. A case near a branch of the return mapping (a singular
value at the clamp, a trial state on the yield surface) could legitimately take the other
branch in float32, so such cases are left out: every case keeps a margin to every decision.

    python3 make_golden.py          # rewrites the JSON; commit it together with port changes
"""
import json
import math
import os

import numpy as np

import test_friction as fr
import test_material_ccc as ccc
import test_material_dp as dp
import test_mpm as snow
from test_svd import svd3

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "unittests", "webgpu_compute", "golden", "mpm_golden.json")
rng = np.random.default_rng(20261107)

E, NU = 1.4e5, 0.2
MU_0 = E / (2 * (1 + NU))
LAMBDA_0 = E * NU / ((1 + NU) * (1 - 2 * NU))


def rotation():
    q, r = np.linalg.qr(rng.normal(size=(3, 3)))
    q = q @ np.diag(np.sign(np.diag(r)))
    if np.linalg.det(q) < 0:
        q[:, 0] = -q[:, 0]
    return q


def deformation(sigma):
    return rotation() @ np.diag(sigma) @ rotation().T


def mat(m):
    return [[float(x) for x in row] for row in m]


# --- SVD -------------------------------------------------------------------------------

def svd_cases():
    fs = [np.eye(3), np.diag([1.2, 0.9, 0.7]), np.diag([1.0, 1.0, 0.8]), deformation([1.1, 1.1, 1.1]),
          np.diag([1.0, 1.0, -1.0]), deformation([1.3, 0.8, -0.5])]
    fs += [deformation(rng.uniform(0.6, 1.4, 3)) for _ in range(40)]
    fs += [np.eye(3) + 0.02 * rng.normal(size=(3, 3)) for _ in range(20)]
    cases = []
    for f in fs:
        _, s, _ = svd3(f)
        cases.append({"F": mat(f), "sigma": [float(x) for x in s]})
    return cases


# --- materials -------------------------------------------------------------------------

def stomakhin_cases():
    lo, hi = 1.0 - snow.CRIT_COMP, 1.0 + snow.CRIT_STRETCH
    cases = []
    tries = [([1.0, 1.0, 1.0], 1.0), ([1.004, 0.99, 0.98], 1.0), ([1.02, 1.0, 0.99], 1.0),
             ([0.95, 0.99, 1.0], 0.9), ([1.05, 0.9, 1.0], 1.2)]
    tries += [(rng.uniform(0.94, 1.03, 3), rng.uniform(0.7, 1.3)) for _ in range(40)]
    tries += [(rng.uniform(lo, hi, 3), rng.uniform(0.7, 1.3)) for _ in range(20)]  # elastic
    for sigma, jp in tries:
        if min(min(abs(s - lo), abs(s - hi)) for s in sigma) < 1e-4:
            continue
        f = deformation(sigma)
        f_new, jp_new = snow.apply_plasticity(f, jp)
        cases.append({"F": mat(f), "state": float(jp), "F_elastic": mat(f_new), "plastic_state": jp_new,
                      "tau": mat(snow.snow_stress(f_new, jp_new)), "yielded": bool(any(s < lo or s > hi for s in sigma))})
    return cases


def dp_cases():
    cases = []
    # Not F = I: the cone apex, where trace and deviator are both rounding noise.
    tries = [[0.99, 0.99, 0.99], [1.01, 1.0, 1.0], [1.002, 0.99, 0.995], [1.02, 0.97, 0.99]]
    tries += [rng.uniform(0.97, 1.02, 3) for _ in range(60)]
    for sigma in tries:
        eps = np.log(sigma)
        trace = eps.sum()
        dev = eps - trace / 3.0
        dg = np.linalg.norm(dev) + (3 * dp.settings.lambda_0 + 2 * dp.settings.mu_0) / (2 * dp.settings.mu_0) * trace * dp.settings.dp_alpha
        if abs(trace) < 1e-5 or abs(dg) < 1e-5:
            continue
        f = deformation(sigma)
        state = float(rng.uniform(0.0, 0.1))
        f_new, state_new, case = dp.dp_plasticity(f, state)
        cases.append({"F": mat(f), "state": state, "F_elastic": mat(f_new), "plastic_state": float(state_new),
                      "tau": mat(dp.dp_stress(f_new)), "yielded": case != 1, "case": case})
    return cases


def ccc_cases(n_random):
    s = ccc.settings
    k, m, b = ccc.bulk(), s.ccc_m, s.ccc_beta
    alpha0 = ccc.ccc_initial_state()
    cases = []
    # Sample the trial strain at the scale of the yield surface: the trace around p0 / K, the
    # deviator around q = M p0, so all four branches occur.
    p_scale = ccc.ccc_p0(alpha0) / k
    q_scale = m * ccc.ccc_p0(alpha0) / (math.sqrt(1.5) * 2.0 * s.mu_0)
    tries = []
    for _ in range(n_random):
        direction = rng.normal(size=3)
        direction -= direction.mean()
        direction /= np.linalg.norm(direction)
        eps = rng.uniform(-1.6, 0.6) * p_scale / 3.0 + rng.uniform(0.0, 1.6) * q_scale * direction
        tries.append((np.exp(eps), alpha0 * rng.uniform(0.7, 1.3)))
    # Compacted states: sinh arguments xi |alpha| from 0.05 to 3, across the switch from the series
    # to sinh() at 0.1 in ccc_sinh. Mostly elastic, since the strain scale is the initial p0's.
    for arg in np.concatenate([np.linspace(0.05, 0.15, 21), rng.uniform(0.15, 3.0, 10)]):
        tries.append((np.exp(rng.normal(scale=0.3 * p_scale, size=3)), -arg / s.ccc_xi))
    for sigma, alpha in tries:
        p0 = ccc.ccc_p0(alpha)
        p, q, _, _ = ccc.invariants(np.log(sigma))
        y = ccc.yield_value(p, q, p0)
        if min(abs(p - p0), abs(p + b * p0)) < 1e-3 * p0 or abs(y) < 1e-3 * (m * p0) ** 2:
            continue
        f = deformation(sigma)
        f_new, alpha_new, case, _ = ccc.ccc_plasticity(f, alpha)
        cases.append({"F": mat(f), "state": float(alpha), "F_elastic": mat(f_new), "plastic_state": float(alpha_new),
                      "tau": mat(ccc.ccc_stress(f_new)), "yielded": case != 0, "case": case,
                      "p0": ccc.ccc_p0(alpha)})
    return cases


def ccc_groups():
    groups = []
    # Li 2021 Table 1, cases II and V, with the app's stiffness and with Li's 3 MPa.
    for name, E_, nu, M, beta, xi, p0 in [("ccc_li_case_v_soft", 1.4e5, 0.2, 0.7, 0.2, 0.002, 3000.0),
                                         ("ccc_li_case_ii", 3e6, 0.3, 1.5, 0.3, 1.0, 30000.0),
                                         ("ccc_li_case_v", 3e6, 0.3, 0.7, 0.2, 0.002, 3000.0)]:
        ccc.settings = ccc.Settings(E=E_, nu=nu, M=M, beta=beta, xi=xi, p0=p0)
        cases = ccc_cases(200)
        groups.append({"name": name, "constitutive_model": 2,
                       "settings": {"mu_0": ccc.settings.mu_0, "lambda_0": ccc.settings.lambda_0, "ccc_m": M, "ccc_beta": beta,
                                    "ccc_xi": xi, "ccc_p0_initial": p0},
                       "initial_state": ccc.ccc_initial_state(), "cases": cases})
    return groups


# --- friction --------------------------------------------------------------------------

def friction_groups():
    groups = []
    normals = [np.array([0.0, 0.0, 1.0]), np.array([-0.5, 0.0, 1.0]), np.array([0.2, -0.4, 1.0])]
    for name, model, mu in [("coulomb", 0, 0.47), ("voellmy", 1, 0.155)]:
        fr.settings.basal_friction_model, fr.settings.terrain_friction = model, mu
        cases = []
        for n in normals:
            n = n / np.linalg.norm(n)
            vels = [np.array([5.0, 0.0, -1.0]), np.array([0.1, 0.0, -2.0]), np.array([3.0, 2.0, 1.0])]
            vels += [rng.normal(scale=10.0, size=3) for _ in range(10)]
            for v in vels:
                vn = float(np.dot(v, n))
                vt = np.linalg.norm(v - n * vn)
                if abs(vn) < 1e-4 or (vn < 0 and abs(vt + mu * vn) < 1e-3):
                    continue  # grazing (contact or not is rounding) or at the stick/slip boundary
                for drag, depth in [(False, 0.0), (True, 0.05), (True, 2.0)]:
                    out = fr.resolve_terrain_collision(v.copy(), n, drag, depth)
                    cases.append({"velocity": v.tolist(), "normal": n.tolist(), "apply_basal_drag": drag,
                                  "flow_depth": depth, "result": [float(x) for x in out]})
        groups.append({"name": name, "basal_friction_model": model,
                       "settings": {"terrain_friction": mu, "voellmy_xi": fr.settings.voellmy_xi, "gravity": fr.settings.gravity,
                                    "dt": fr.settings.dt},
                       "cases": cases})
    return groups


# --- B-spline weights -----------------------------------------------------------------

def kernel_cases():
    cases = []
    for gp in [np.array([3.5, 3.5, 3.5]), np.array([3.0, 4.25, 7.9]), np.array([10.49, 10.51, 2.0])] + \
              [rng.uniform(2.0, 20.0, 3) for _ in range(20)]:
        base, fx, w = snow.compute_kernel(gp)
        cases.append({"grid_pos": gp.tolist(), "base": [int(x) for x in base], "fx": fx.tolist(),
                      "w": [[float(x) for x in wi] for wi in w]})
    return cases


def main():
    golden = {
        "comment": "Generated by mpm-mls-doc/scripts/make_golden.py from the Python ports; do not edit.",
        "svd": svd_cases(),
        "materials": [
            {"name": "stomakhin", "constitutive_model": 0,
             "settings": {"mu_0": snow.MU_0, "lambda_0": snow.LAMBDA_0, "hardening": snow.HARDENING,
                          "critical_compression": snow.CRIT_COMP, "critical_stretch": snow.CRIT_STRETCH},
             "initial_state": 1.0, "cases": stomakhin_cases()},
            {"name": "drucker_prager", "constitutive_model": 1,
             "settings": {"mu_0": dp.settings.mu_0, "lambda_0": dp.settings.lambda_0, "dp_alpha": dp.settings.dp_alpha},
             "initial_state": 0.0, "cases": dp_cases()},
        ] + ccc_groups(),
        "friction": friction_groups(),
        "kernel": kernel_cases(),
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(golden, f, indent=1)
    counts = {g["name"]: len(g["cases"]) for g in golden["materials"] + golden["friction"]}
    print(f"svd {len(golden['svd'])}, kernel {len(golden['kernel'])}, {counts}")
    for g in golden["materials"]:
        if "case" in (g["cases"] or [{}])[0]:
            print(g["name"], "cases by branch:", sorted({c["case"] for c in g["cases"]}),
                  {k: sum(c["case"] == k for c in g["cases"]) for k in sorted({c["case"] for c in g["cases"]})})
        else:
            print(g["name"], "yielded:", sum(c["yielded"] for c in g["cases"]), "of", len(g["cases"]))
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
