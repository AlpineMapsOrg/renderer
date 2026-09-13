#!/usr/bin/env python3
"""Energy-line test of the coupled solver (Tonnel et al. 2023, com1DFA section 5.2).

A slab of snow is placed on an inclined plane and slides. Along the centre-of-mass path,
Coulomb friction removes exactly mu of *energy height*  h_E = z + v^2/(2g)  per horizontal
metre, whatever the slope geometry. So the least-squares slope of h_E against horizontal
distance is -mu_eff, and mu_eff - mu is whatever the material dissipates internally.

This checks that the driving and resisting forces of the whole loop - gravity, the grid
transfers, the constitutive model and the basal friction boundary condition - balance the
way they should, without needing a real avalanche to compare against.

Two runs on the same slope: mu = 0.3 and mu = 0. The difference of the two mu_eff isolates
the basal friction contribution from the internal one and must come out as ~0.3.

    python3 test_energy_line.py
"""
import numpy as np

import test_mpm as mpm

G = mpm.GRAVITY


def run(mu, slope_deg, n_particles=60, steps=1600, seed=5):
    """Slide a slab; return (path, energy_height) samples of the centre of mass."""
    mpm.FRICTION = mu
    mpm.SLOPE_DEG = slope_deg
    # Terrain rises with +x, so downhill is -x: seed near the top and give it ~50 m of
    # runway. (The first version seeded at x = 4..9 and slid straight into the wall at x = 2.)
    mpm.GRID = np.array([64, 24, 48])
    rng = np.random.default_rng(seed)

    # Seed the slab resting on the surface (not dropped) to avoid an impact transient.
    x = rng.uniform(52.0, 57.0, n_particles)
    y = rng.uniform(9.0, 14.0, n_particles)
    z = mpm.terrain_height(x) + rng.uniform(0.05, 0.6, n_particles)
    pos = np.column_stack([x, y, z])
    vel = np.zeros((n_particles, 3))
    C = np.zeros((n_particles, 3, 3))
    F = np.array([np.eye(3)] * n_particles)
    jp = np.full(n_particles, mpm.material_initial_state())

    path, heights, times = [], [], []
    prev_com_xy, s = None, 0.0
    for step in range(steps):
        mpm.substep(pos, vel, C, F, jp)
        if step % 20:
            continue
        com = pos.mean(axis=0)
        if prev_com_xy is not None:
            s += np.linalg.norm(com[:2] - prev_com_xy)
        prev_com_xy = com[:2]
        mean_v2 = (vel * vel).sum(axis=1).mean()
        path.append(s)
        heights.append(com[2] + mean_v2 / (2 * G))
        times.append(step * mpm.DT)
        if not np.isfinite(pos).all():
            raise RuntimeError(f"diverged at step {step}")
    return np.array(path), np.array(heights), np.array(times)


def fit_mu_eff(path, heights, skip_m=0.5):
    """-slope of energy height over path, ignoring the initial settling onto the slope."""
    mask = path > skip_m
    if mask.sum() < 3:
        return float("nan")
    slope, _ = np.polyfit(path[mask], heights[mask], 1)
    return -slope


def main():
    ok = True
    slope_deg = 30.0
    mu_set = 0.3
    print(f"model: {mpm.MODEL}, slope {slope_deg} deg, Coulomb basal friction")
    print("(mu = 0.3 slides: tan(30 deg) = 0.577 > 0.3)\n")

    results = {}
    for mu in (mu_set, 0.0):
        path, heights, times = run(mu, slope_deg)
        mu_eff = fit_mu_eff(path, heights)
        results[mu] = mu_eff
        print(f"mu = {mu:.1f}:  travelled {path[-1]:6.2f} m in {times[-1]:.2f} s,  "
              f"energy height {heights[0]:.2f} -> {heights[-1]:.2f} m,  mu_eff = {mu_eff:.4f}")

        # Sanity on the free-slide kinematics: acceleration along the slope should be
        # close to g (sin - mu cos) once sliding; over-estimate is what internal
        # dissipation would produce, under-estimate would mean spurious driving.
        a_expected = G * (np.sin(np.radians(slope_deg)) - mu * np.cos(np.radians(slope_deg)))
        s_expected = 0.5 * a_expected * times[-1] ** 2
        print(f"           free-slide distance would be {s_expected:6.2f} m "
              f"({path[-1] / s_expected:.0%} of it)")

    internal = results[0.0]
    basal = results[mu_set] - results[0.0]
    print(f"\ninternal dissipation (mu = 0 run):        {internal:+.4f}")
    print(f"basal contribution  (mu 0.3 minus mu 0):  {basal:+.4f}   expected {mu_set:.1f}")

    if not np.isfinite(basal):
        print("FAIL: could not fit"); ok = False
    if abs(basal - mu_set) > 0.2 * mu_set:
        print(f"FAIL: basal friction contribution {basal:.3f} is not within 20 % of the set mu {mu_set}"); ok = False
    if internal < -0.02:
        print(f"FAIL: negative internal dissipation {internal:.3f} - energy is being created"); ok = False
    if internal > 0.15:
        print(f"FAIL: internal dissipation {internal:.3f} is implausibly large for a sliding slab"); ok = False

    print("\nALL OK" if ok else "\nFAILURES PRESENT")
    print("Expected: mu_eff(0.3) - mu_eff(0) ~ 0.3 - the friction law removes exactly mu of energy\n"
          "height per horizontal metre; internal dissipation small and non-negative.")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
