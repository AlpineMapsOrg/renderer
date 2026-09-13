#!/usr/bin/env python3
"""Device-scale sheet test: a thin snow slab (1.5 m) on an inclined plane in 12.5 m cells,
with the shader's fixed-point grid accumulation emulated.

This is the test that reproduced the "simulation never cools down" blow-up of 2026-09-13.
The plane result is exact in floating point at any dx, but with the *old* accumulation
scheme (truncate every contribution to 1e-4, one i32 for the mass) the slab runs away
exponentially: at low-weight nodes - the third z layer of a slab that is 0.12 cells thick
- truncation shrinks the mass by more than the momentum, momentum / mass comes out too
large, and APIC's C hands the excess back to the particles every step. Independent of dt,
of the constitutive model and of the particle count, tangential only: exactly the symptoms
seen on the device.

The shader now rounds and keeps the mass at 2^-20 in a 64-bit accumulator. Emulating that
here must match the float reference; the old scheme is printed for comparison.

    python3 test_sheet_fixed_point.py            # ~4 min: float, new scheme, old scheme
    QUICK=1 python3 test_sheet_fixed_point.py    # float + new scheme only

Vectorised (numpy) rewrite of the test_mpm.py loop, Stomakhin model, Coulomb friction.
"""
import math
import os
import sys

import numpy as np

G = 9.81


def run(dx=12.5, dt=0.01, slope=35.0, mu=0.47, thick=1.5, per_m3=2.0, patch_cells=3, T=20.0,
        E=1.4e5, nu=0.2, rho=400.0, hard=10.0, tc=2.5e-2, ts=7.5e-3,
        momentum_scale=0.0, mass_scale=0.0, rounding=True, grid=(64, 12, 64), seed=1, report=4.0):
    """Returns (t, mean speed) samples of a sliding slab. momentum_scale / mass_scale = 0
    means exact floats; otherwise every P2G contribution is quantised like the shader."""
    mu0 = E / (2 * (1 + nu))
    lam0 = E * nu / ((1 + nu) * (1 - 2 * nu))
    vol = 1.0 / rho
    tan_s = math.tan(math.radians(slope))
    nrm = np.array([-math.sin(math.radians(slope)), 0.0, math.cos(math.radians(slope))])
    grid = np.array(grid)
    floor = 3.0 * dx
    height = lambda x: floor + tan_s * x
    quant = np.round if rounding else np.trunc

    rng = np.random.default_rng(seed)
    patch = patch_cells * dx
    n = int(per_m3 * patch * patch * thick)
    x0 = (grid[0] - 6) * dx - patch  # near the top: the slab slides towards -x
    x = rng.uniform(x0, x0 + patch, n)
    y = rng.uniform(4 * dx, 4 * dx + patch, n)
    z = height(x) + rng.uniform(0, thick, n)
    pos = np.column_stack([x, y, z])
    vel = np.zeros((n, 3))
    C = np.zeros((n, 3, 3))
    F = np.tile(np.eye(3), (n, 1, 1))
    jp = np.ones(n)
    offsets = np.array([[i, j, l] for i in range(3) for j in range(3) for l in range(3)])

    node_x = np.arange(grid[0]) * dx
    node_below = np.zeros(tuple(grid), bool)
    for k in range(grid[2]):
        node_below[:, :, k] = (k * dx < height(node_x))[:, None]

    def coulomb(v):
        vn = v @ nrm
        into = vn < 0
        vt = v - np.outer(vn, nrm)
        vt_len = np.linalg.norm(vt, axis=1)
        stick = into & (vt_len <= -mu * vn)
        slide = into & ~stick
        v = v.copy()
        v[stick] = 0
        v[slide] = vt[slide] * (1.0 + mu * vn[slide] / vt_len[slide])[:, None]
        return v

    samples = []
    steps, every = int(round(T / dt)), int(round(report / dt))
    for step in range(steps):
        gp = pos / dx
        base = np.floor(gp - 0.5).astype(int)
        fx = gp - base
        w = np.stack([0.5 * (1.5 - fx) ** 2, 0.75 - (fx - 1.0) ** 2, 0.5 * (fx - 0.5) ** 2])

        # --- P2G (Stomakhin stress, MLS-MPM force term) ---
        U, S, Vh = np.linalg.svd(F)
        h = np.clip(np.exp(hard * (1.0 - jp)), 0.05, 20.0)
        R = U @ Vh
        J = S.prod(axis=1)
        stress = (2.0 * (mu0 * h)[:, None, None] * (F - R) @ np.transpose(F, (0, 2, 1))
                  + (lam0 * h * J * (J - 1.0))[:, None, None] * np.eye(3))
        affine = -dt * vol * 4.0 / dx / dx * stress + C
        gm = np.zeros(tuple(grid))
        gv = np.zeros(tuple(grid) + (3,))
        for o in offsets:
            node = base + o
            wt = w[o[0], :, 0] * w[o[1], :, 1] * w[o[2], :, 2]
            dpos = (o - fx) * dx
            mom = wt[:, None] * (vel + np.einsum('nij,nj->ni', affine, dpos))
            if mass_scale > 0:
                wt = quant(wt * mass_scale) / mass_scale
            if momentum_scale > 0:
                mom = quant(mom * momentum_scale) / momentum_scale
            idx = (node[:, 0], node[:, 1], node[:, 2])
            np.add.at(gm, idx, wt)
            np.add.at(gv, idx, mom)

        # --- grid update ---
        occupied = gm > 0
        v = np.zeros_like(gv)
        v[occupied] = gv[occupied] / gm[occupied][:, None]
        v[..., 2] -= G * dt
        v[~occupied] = 0
        below = occupied & node_below
        v[below] = coulomb(v[below])
        gv = v

        # --- G2P ---
        new_v = np.zeros_like(vel)
        new_C = np.zeros_like(C)
        for o in offsets:
            node = base + o
            wt = w[o[0], :, 0] * w[o[1], :, 1] * w[o[2], :, 2]
            g_v = gv[node[:, 0], node[:, 1], node[:, 2]]
            new_v += wt[:, None] * g_v
            new_C += 4.0 / dx * wt[:, None, None] * np.einsum('ni,nj->nij', g_v, o - fx)
        vel, C = new_v, new_C
        U, S, Vh = np.linalg.svd((np.eye(3) + dt * C) @ F)
        Sc = np.clip(S, 1 - tc, 1 + ts)
        jp = np.clip(jp * (S / Sc).prod(axis=1), 0.05, 20.0)
        F = U @ (Sc[:, :, None] * Vh)
        pos = pos + dt * vel
        surface = height(pos[:, 0])
        under = pos[:, 2] < surface
        pos[under, 2] = surface[under]
        if under.any():
            vel[under] = coulomb(vel[under])

        if (step + 1) % every == 0:
            speed = np.linalg.norm(vel, axis=1)
            samples.append(((step + 1) * dt, speed.mean(), speed.max()))
            if not np.isfinite(vel).all() or pos[:, 0].min() < 3 * dx:
                break
    return samples


def main():
    slope, mu = 35.0, 0.47
    a = G * (math.sin(math.radians(slope)) - mu * math.cos(math.radians(slope)))
    configs = [("float reference", dict()),
               ("shader (round, momentum 1e4, mass 2^20)", dict(momentum_scale=1e4, mass_scale=2.0 ** 20))]
    if not os.environ.get("QUICK"):
        configs.append(("old scheme (truncate, both 1e4)", dict(momentum_scale=1e4, mass_scale=1e4, rounding=False)))

    print(f"1.5 m slab, dx 12.5 m, dt 0.01 s, {slope} deg plane, Coulomb mu {mu}: rigid block a = {a:.3f} m/s^2\n")
    ok = True
    for name, kw in configs:
        samples = run(slope=slope, mu=mu, **kw)
        print(name)
        print(f"  {'t':>5} {'mean|v|':>8} {'max|v|':>8} {'analytic':>9}")
        for t, mean, vmax in samples:
            print(f"  {t:5.1f} {mean:8.2f} {vmax:8.2f} {a * t:9.2f}")
        t, mean, vmax = samples[-1]
        err = mean / (a * t) - 1.0
        is_old = "old" in name
        verdict = "(the bug)" if is_old else ("OK" if abs(err) < 0.03 else "FAIL")
        if not is_old and abs(err) >= 0.03:
            ok = False
        print(f"  -> mean speed {err:+.1%} of the analytic block at t = {t:.0f} s  {verdict}\n")

    print("ALL OK" if ok else "FAILURES PRESENT")
    print("Expected: float and shader scheme within 3 % of the block; the old scheme runs away.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
