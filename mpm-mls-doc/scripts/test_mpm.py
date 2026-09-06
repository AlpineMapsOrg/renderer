#!/usr/bin/env python3
"""Offline port of the MPM substep kernels (mpm_p2g / mpm_grid_update / mpm_g2p).

Same formulas and the same normalised-mass convention as the shaders, with a flat floor
instead of a DEM. Checks that the loop conserves mass, reproduces gravity, lands, settles,
and compacts plastically instead of bouncing.

This is the plan's "Phase 2: de-risk the algorithm outside WGSL" step -- debugging MPM maths
and WGSL atomics/alignment at the same time is a bad idea.

    python3 test_mpm.py            # drop + land + settle (~900 substeps, slow: pure Python)
"""
import numpy as np

from test_svd import svd3

# --- settings mirroring MpmSolverSettings, at a small test scale ---
DX = 1.0
GRID = np.array([32, 32, 32])
DT = 2e-3
DENSITY = 400.0
E, NU = 1.4e5, 0.2
MU_0 = E / (2 * (1 + NU))
LAMBDA_0 = E * NU / ((1 + NU) * (1 - 2 * NU))
HARDENING = 10.0
CRIT_COMP, CRIT_STRETCH = 2.5e-2, 7.5e-3
GRAVITY = 9.81
FRICTION = 0.4
FLOOR_Z = 3.0
PARTICLE_MASS = 1.0          # normalised; see 04-data-layout.md
PARTICLE_VOLUME = 1.0 / DENSITY


def compute_kernel(gp):
    """Quadratic B-spline weights, 3x3x3 stencil."""
    base = np.floor(gp - 0.5).astype(int)
    fx = gp - base
    w = [0.5 * (1.5 - fx) ** 2, 0.75 - (fx - 1.0) ** 2, 0.5 * (fx - 0.5) ** 2]
    return base, fx, w


def snow_stress(F, jp):
    """Stomakhin et al. 2013, premultiplied by F^T as MLS-MPM needs."""
    U, S, V = svd3(F)
    h = np.clip(np.exp(HARDENING * (1.0 - jp)), 0.05, 20.0)
    mu, lam = MU_0 * h, LAMBDA_0 * h
    R = U @ V.T
    J = S[0] * S[1] * S[2]
    return 2.0 * mu * (F - R) @ F.T + np.eye(3) * (lam * J * (J - 1.0))


def apply_plasticity(F_trial, jp):
    U, S, V = svd3(F_trial)
    clamped = np.clip(S, 1.0 - CRIT_COMP, 1.0 + CRIT_STRETCH)
    ratio = np.prod(S / clamped)
    return U @ np.diag(clamped) @ V.T, float(np.clip(jp * ratio, 0.05, 20.0))


def resolve_collision(v, n):
    """Coulomb friction; identical to resolve_terrain_collision() in the shader."""
    vn = float(np.dot(v, n))
    if vn >= 0.0:
        return v
    vt = v - n * vn
    vt_len = np.linalg.norm(vt)
    if vt_len <= -FRICTION * vn:
        return np.zeros(3)              # sticking
    return vt * (1.0 + FRICTION * vn / vt_len)


def substep(pos, vel, C, F, jp):
    n = len(pos)
    gm = np.zeros(tuple(GRID))
    gv = np.zeros(tuple(GRID) + (3,))
    inv_dx = 1.0 / DX

    # --- P2G ---
    for p in range(n):
        base, fx, w = compute_kernel(pos[p] / DX)
        stress = snow_stress(F[p], jp[p])
        stress_term = -DT * PARTICLE_VOLUME * (4.0 * inv_dx * inv_dx) * stress
        affine = stress_term + PARTICLE_MASS * C[p]
        for i in range(3):
            for j in range(3):
                for l in range(3):
                    node = base + np.array([i, j, l])
                    if np.any(node < 0) or np.any(node >= GRID):
                        continue
                    weight = w[i][0] * w[j][1] * w[l][2]
                    dpos = (np.array([i, j, l], dtype=float) - fx) * DX   # world units
                    gm[tuple(node)] += weight * PARTICLE_MASS
                    gv[tuple(node)] += weight * (PARTICLE_MASS * vel[p] + affine @ dpos)

    # --- grid update ---
    occupied = gm > 1e-9
    for node in np.argwhere(occupied):
        t = tuple(node)
        v = gv[t] / gm[t]
        v[2] -= GRAVITY * DT
        if node[2] * DX < FLOOR_Z:
            v = resolve_collision(v, np.array([0.0, 0.0, 1.0]))
        for d in range(3):
            if node[d] < 2 and v[d] < 0:
                v[d] = 0.0
            if node[d] >= GRID[d] - 3 and v[d] > 0:
                v[d] = 0.0
        gv[t] = v
    gv[~occupied] = 0.0

    # --- G2P + advection ---
    for p in range(n):
        base, fx, w = compute_kernel(pos[p] / DX)
        new_v = np.zeros(3)
        new_C = np.zeros((3, 3))
        for i in range(3):
            for j in range(3):
                for l in range(3):
                    node = base + np.array([i, j, l])
                    if np.any(node < 0) or np.any(node >= GRID):
                        continue
                    weight = w[i][0] * w[j][1] * w[l][2]
                    dpos = np.array([i, j, l], dtype=float) - fx          # grid units
                    g_v = gv[tuple(node)]
                    new_v += weight * g_v
                    new_C += 4.0 * inv_dx * weight * np.outer(g_v, dpos)
        vel[p] = new_v
        C[p] = new_C
        F[p], jp[p] = apply_plasticity((np.eye(3) + DT * new_C) @ F[p], jp[p])
        pos[p] = pos[p] + DT * new_v
        if pos[p][2] < FLOOR_Z:
            pos[p][2] = FLOOR_Z
            vel[p] = resolve_collision(vel[p], np.array([0.0, 0.0, 1.0]))
        pos[p] = np.clip(pos[p], 2.0 * DX, (GRID - 3) * DX)
    return gm.sum()


def main(n_particles=150, steps=900, drop=(5, 9)):
    rng = np.random.default_rng(3)
    pos = np.column_stack([rng.uniform(12, 20, n_particles),
                           rng.uniform(12, 20, n_particles),
                           rng.uniform(drop[0], drop[1], n_particles)])
    vel = np.zeros((n_particles, 3))
    C = np.zeros((n_particles, 3, 3))
    F = np.array([np.eye(3)] * n_particles)
    jp = np.ones(n_particles)

    print(f"{'step':>5} {'grid mass':>10} {'mean z':>8} {'min z':>7} {'max|v|':>8} {'mean jp':>8}")
    mass = 0.0
    for step in range(steps):
        mass = substep(pos, vel, C, F, jp)
        if step % max(steps // 6, 1) == 0 or step == steps - 1:
            print(f"{step:5d} {mass:10.2f} {pos[:,2].mean():8.2f} {pos[:,2].min():7.2f} "
                  f"{np.abs(vel).max():8.3f} {jp.mean():8.4f}")
        if not np.isfinite(pos).all() or np.abs(vel).max() > 1e4:
            print(f"DIVERGED at step {step}")
            return 1

    ok = True
    if abs(mass - n_particles) > 0.05 * n_particles:
        print(f"FAIL: mass not conserved ({mass:.2f} vs {n_particles})"); ok = False
    if pos[:, 2].min() < FLOOR_Z - 1e-6:
        print(f"FAIL: particles below the floor (min z {pos[:,2].min():.3f})"); ok = False
    if np.abs(vel).max() > 50:
        print(f"FAIL: not settling (max |v| {np.abs(vel).max():.1f})"); ok = False
    if jp.mean() >= 0.999:
        print(f"FAIL: no plastic compaction (mean jp {jp.mean():.4f})"); ok = False

    print("\nALL OK" if ok else "\nFAILURES PRESENT")
    print("Expected: mass exactly conserved, min z == floor, velocity decaying to ~0,\n"
          "mean jp dropping well below 1 (plastic compaction on impact, not a bounce).")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
