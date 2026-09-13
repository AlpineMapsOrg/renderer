#!/usr/bin/env python3
"""Verbatim port of mpm_material_drucker_prager.wgsl, checked against the yield surface.

The whole point of a return mapping is that its output satisfies the yield condition. For
Drucker-Prager that condition is closed-form, so it can be checked to floating-point
precision after every projection:

    y(tau) = |dev(tau)| + alpha * tr(tau)   must be <= 0, and == 0 for a Case III projection

Also pins the three cases of Klar et al. 2016 section 5.3, including the one that is easy to
get wrong: pure hydrostatic compression has zero deviatoric strain but sits inside the
cone and must stay elastic, not be projected to the tip.

    python3 test_material_dp.py
"""
import math

import numpy as np

from test_svd import svd3


class Settings:
    E, nu = 1.4e5, 0.2
    mu_0 = E / (2 * (1 + nu))
    lambda_0 = E * nu / ((1 + nu) * (1 - 2 * nu))
    dp_friction_angle = 30.0
    sin_phi = math.sin(math.radians(dp_friction_angle))
    dp_alpha = math.sqrt(2.0 / 3.0) * 2.0 * sin_phi / (3.0 - sin_phi)


settings = Settings()


# --- port of mpm_material_drucker_prager.wgsl -----------------------------------------

def dp_hencky_strain(sigma):
    return np.log(np.clip(sigma, 1e-3, 1e3))


def dp_stress(F):
    U, S, V = svd3(F)
    eps = dp_hencky_strain(S)
    trace = eps.sum()
    tau = 2.0 * settings.mu_0 * eps + settings.lambda_0 * trace
    return U @ np.diag(tau) @ U.T


def dp_plasticity(F_trial, plastic_state):
    U, S, V = svd3(F_trial)
    eps = dp_hencky_strain(S)
    trace = eps.sum()
    dev = eps - trace / 3.0
    dev_norm = np.linalg.norm(dev)

    new_eps = eps.copy()
    delta_gamma = 0.0
    if trace > 0.0:
        new_eps = np.zeros(3)                      # Case II
        delta_gamma = np.linalg.norm(eps)
        case = 2
    else:
        delta_gamma = dev_norm + (3.0 * settings.lambda_0 + 2.0 * settings.mu_0) / (2.0 * settings.mu_0) * trace * settings.dp_alpha
        if delta_gamma <= 0.0:
            delta_gamma = 0.0                      # Case I
            case = 1
        else:
            new_eps = eps - delta_gamma * dev / dev_norm   # Case III
            case = 3
    F_new = U @ np.diag(np.exp(new_eps)) @ V.T
    return F_new, plastic_state + delta_gamma, case


# --- yield function on a stress tensor -----------------------------------------------

def yield_value(tau):
    trace = np.trace(tau)
    dev = tau - np.eye(3) * trace / 3.0
    return np.linalg.norm(dev, "fro") + settings.dp_alpha * trace


def main():
    ok = True
    rng = np.random.default_rng(11)

    def report(label, cond, detail=""):
        nonlocal ok
        print(f"{'OK  ' if cond else 'FAIL'} {label} {detail}")
        ok &= cond

    # 0. Undeformed: zero stress, elastic, unchanged.
    F = np.eye(3)
    report("F = I gives zero stress", np.allclose(dp_stress(F), 0))
    Fn, ps, case = dp_plasticity(F, 0.0)
    report("F = I is Case I", case == 1 and ps == 0.0 and np.allclose(Fn, F))

    # 1. Pure hydrostatic compression: inside the cone, must stay elastic (the bug caught
    #    during implementation sent this to the tip).
    F = np.eye(3) * 0.95
    Fn, ps, case = dp_plasticity(F, 0.0)
    report("hydrostatic compression stays elastic", case == 1 and ps == 0.0 and np.allclose(Fn, F),
           f"case={case} y={yield_value(dp_stress(F)):+.3e}")

    # 2. Hydrostatic expansion: tension, cohesionless -> Case II, all strain dropped.
    F = np.eye(3) * 1.05
    Fn, ps, case = dp_plasticity(F, 0.0)
    report("expansion projects to the tip", case == 2 and np.allclose(Fn, np.eye(3)) and ps > 0,
           f"case={case} plastic={ps:.4f}")

    # 3. Small shear under compression: inside the cone -> elastic.
    gamma = 0.002
    F = np.array([[1.0, gamma, 0], [0, 1.0, 0], [0, 0, 1.0]]) @ (np.eye(3) * 0.98)
    y_before = yield_value(dp_stress(F))
    Fn, ps, case = dp_plasticity(F, 0.0)
    report("small shear inside cone is elastic", case == 1 and y_before < 0 and np.allclose(Fn, F),
           f"y={y_before:+.3e}")

    # 4. Large shear under compression: outside the cone -> Case III, and the projected
    #    stress must lie ON the cone, y == 0.
    gamma = 0.3
    F = np.array([[1.0, gamma, 0], [0, 1.0, 0], [0, 0, 1.0]]) @ (np.eye(3) * 0.98)
    y_before = yield_value(dp_stress(F))
    Fn, ps, case = dp_plasticity(F, 0.0)
    y_after = yield_value(dp_stress(Fn))
    report("large shear is Case III", case == 3 and y_before > 0 and ps > 0, f"y_before={y_before:+.3e}")
    report("projected stress lies on the cone", abs(y_after) < 1e-6 * settings.E, f"y_after={y_after:+.3e}")

    # 5. Idempotence: re-projecting a state that is already on the cone must not move it.
    #    y is ~1e-12 there, so which case label fires is floating-point noise - what matters
    #    is that F and the plastic state do not change materially.
    Fn2, ps2, case2 = dp_plasticity(Fn, ps)
    report("projection is idempotent", np.allclose(Fn2, Fn, atol=1e-9) and abs(ps2 - ps) < 1e-9,
           f"case={case2} |dF|={np.abs(Fn2 - Fn).max():.1e} dplastic={ps2 - ps:.1e}")

    # 6. Sweep: for 500 random trial gradients, the output always satisfies y <= 0 and
    #    Case III outputs land on the surface.
    worst_violation, worst_surface, counts = 0.0, 0.0, {1: 0, 2: 0, 3: 0}
    for _ in range(500):
        F = np.eye(3) + 0.25 * rng.standard_normal((3, 3))
        Fn, ps, case = dp_plasticity(F, 0.0)
        counts[case] += 1
        y = yield_value(dp_stress(Fn))
        worst_violation = max(worst_violation, y)
        if case == 3:
            worst_surface = max(worst_surface, abs(y))
    report("random sweep: never outside the cone", worst_violation < 1e-6 * settings.E,
           f"max y={worst_violation:+.3e}  cases I/II/III = {counts[1]}/{counts[2]}/{counts[3]}")
    report("random sweep: Case III always on the surface", worst_surface < 1e-6 * settings.E,
           f"max |y|={worst_surface:.3e}")

    # 7. Stress symmetry and rotation invariance: rotating F rotates tau.
    F = np.array([[1.1, 0.05, 0], [0, 0.95, 0], [0, 0, 1.0]])
    R = svd3(rng.standard_normal((3, 3)))[0]      # a random proper rotation
    tau, tau_r = dp_stress(F), dp_stress(R @ F)
    report("stress is symmetric", np.allclose(tau, tau.T))
    report("stress rotates with F", np.allclose(tau_r, R @ tau @ R.T, atol=1e-6 * settings.E))

    print("\nALL OK" if ok else "\nFAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
