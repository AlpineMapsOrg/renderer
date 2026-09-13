#!/usr/bin/env python3
"""Verbatim port of mpm_material_ccc.wgsl, checked against the yield surface and hardening law.

Cohesive Cam Clay (Gaume et al. 2018; return mapping per Wolper et al. 2019). Everything
is closed-form, so every projection can be checked directly:

    y(p, q) = (1 + 2 beta) q^2 + M^2 (p + beta p0)(p - p0)   must be <= 0 after projection
    p0(alpha) = K sinh(xi max(-alpha, 0))                    monotone: compaction hardens

Also pins the Li et al. 2021 Table 1 regime parameters: every case must produce a finite
initial state whose p0 round-trips to the configured value.

    python3 test_material_ccc.py
"""
import math

import numpy as np

from test_svd import svd3


class Settings:
    def __init__(self, E=1.4e5, nu=0.2, M=0.7, beta=0.2, xi=0.002, p0=3000.0):
        self.mu_0 = E / (2 * (1 + nu))
        self.lambda_0 = E * nu / ((1 + nu) * (1 - 2 * nu))
        self.E = E
        self.ccc_m, self.ccc_beta, self.ccc_xi, self.ccc_p0_initial = M, beta, xi, p0


settings = Settings()


# --- port of mpm_material_ccc.wgsl ---------------------------------------------------

def bulk():
    return settings.lambda_0 + 2.0 * settings.mu_0 / 3.0


def ccc_p0(alpha):
    arg = min(max(settings.ccc_xi * max(-alpha, 0.0), 0.0), 20.0)
    return bulk() * math.sinh(arg)


def ccc_initial_state():
    return -math.asinh(settings.ccc_p0_initial / bulk()) / max(settings.ccc_xi, 1e-6)


def hencky(sigma):
    return np.log(np.clip(sigma, 1e-3, 1e3))


def ccc_stress(F):
    U, S, V = svd3(F)
    eps = hencky(S)
    tau = 2.0 * settings.mu_0 * eps + settings.lambda_0 * eps.sum()
    return U @ np.diag(tau) @ U.T


def invariants(eps):
    trace = eps.sum()
    dev = eps - trace / 3.0
    p = -bulk() * trace
    q = math.sqrt(1.5) * 2.0 * settings.mu_0 * np.linalg.norm(dev)
    return p, q, trace, dev


def yield_value(p, q, p0):
    b, m = settings.ccc_beta, settings.ccc_m
    return (1.0 + 2.0 * b) * q * q + m * m * (p + b * p0) * (p - p0)


def ccc_plasticity(F_trial, alpha):
    U, S, V = svd3(F_trial)
    eps = hencky(S)
    p, q, trace, dev = invariants(eps)
    k, m, b = bulk(), settings.ccc_m, settings.ccc_beta
    p0 = ccc_p0(alpha)

    new_eps, new_alpha, case = eps.copy(), alpha, 0
    if p > p0:
        new_eps = np.full(3, -p0 / (3.0 * k)); new_alpha = alpha + (trace + p0 / k); case = 1
    elif p < -b * p0:
        new_eps = np.full(3, b * p0 / (3.0 * k)); new_alpha = alpha + (trace - b * p0 / k); case = 2
    else:
        if yield_value(p, q, p0) > 0.0:
            q_new = m * math.sqrt(max((p + b * p0) * (p0 - p) / (1.0 + 2.0 * b), 0.0))
            new_eps = trace / 3.0 + dev * (q_new / max(q, 1e-12)); case = 3
    return U @ np.diag(np.exp(new_eps)) @ V.T, new_alpha, case, p0


def state_of(F):
    """(p, q) of an elastic deformation gradient."""
    _, S, _ = svd3(F)
    p, q, _, _ = invariants(hencky(S))
    return p, q


def main():
    global settings
    ok = True
    rng = np.random.default_rng(23)

    def report(label, cond, detail=""):
        nonlocal ok
        print(f"{'OK  ' if cond else 'FAIL'} {label} {detail}")
        ok &= cond

    tol = 1e-6 * settings.E

    # 1. Initial state round-trips to the configured consolidation pressure.
    a0 = ccc_initial_state()
    report("p0(initial alpha) == p0_initial", abs(ccc_p0(a0) - settings.ccc_p0_initial) < 1e-6,
           f"alpha0={a0:.4f} p0={ccc_p0(a0):.3f}")

    # 2. Undeformed: zero stress, elastic, state untouched.
    Fn, a, case, _ = ccc_plasticity(np.eye(3), a0)
    report("F = I elastic, zero stress", case == 0 and a == a0 and np.allclose(ccc_stress(np.eye(3)), 0))

    # 3. Small strain inside the ellipse stays elastic.
    F = np.array([[1.0, 1e-4, 0], [0, 1.0, 0], [0, 0, 1.0]]) @ (np.eye(3) * 0.999)
    p, q = state_of(F)
    Fn, a, case, p0 = ccc_plasticity(F, a0)
    report("inside ellipse is elastic", case == 0 and yield_value(p, q, p0) < 0 and np.allclose(Fn, F),
           f"y={yield_value(p, q, p0):+.3e}")

    # 4. Shear failure (Case 3): projected state lies ON the ellipse at the SAME p.
    F = np.array([[1.0, 0.05, 0], [0, 1.0, 0], [0, 0, 1.0]]) @ (np.eye(3) * 0.995)
    p_tr, q_tr = state_of(F)
    Fn, a, case, p0 = ccc_plasticity(F, a0)
    p_new, q_new = state_of(Fn)
    report("shear is Case 3", case == 3 and yield_value(p_tr, q_tr, p0) > 0, f"y_tr={yield_value(p_tr, q_tr, p0):+.3e}")
    report("  projected onto the ellipse", abs(yield_value(p_new, q_new, p0)) < tol * tol, f"y={yield_value(p_new, q_new, p0):+.3e}")
    report("  at fixed p", abs(p_new - p_tr) < tol, f"p {p_tr:.3f} -> {p_new:.3f}")
    report("  no hardening from shear", a == a0)

    # 5. Compressive cap (Case 1): return to (p0, 0), alpha decreases, p0 then increases.
    F = np.eye(3) * math.exp(-(settings.ccc_p0_initial * 3.0) / (3.0 * bulk()))  # p = 3 p0
    p_tr, _ = state_of(F)
    Fn, a, case, p0 = ccc_plasticity(F, a0)
    p_new, q_new = state_of(Fn)
    report("compression beyond cap is Case 1", case == 1 and p_tr > p0, f"p_tr={p_tr:.0f} p0={p0:.0f}")
    report("  returned to (p0, 0)", abs(p_new - p0) < tol and q_new < tol, f"p={p_new:.3f} q={q_new:.2e}")
    report("  hardens: alpha down, p0 up", a < a0 and ccc_p0(a) > p0, f"p0 {p0:.1f} -> {ccc_p0(a):.1f}")

    # 6. Tensile failure (Case 2): return to (-beta p0, 0), alpha increases, p0 then decreases.
    F = np.eye(3) * math.exp((settings.ccc_beta * settings.ccc_p0_initial * 3.0) / (3.0 * bulk()))  # p = -3 beta p0
    p_tr, _ = state_of(F)
    Fn, a, case, p0 = ccc_plasticity(F, a0)
    p_new, q_new = state_of(Fn)
    report("tension beyond strength is Case 2", case == 2 and p_tr < -settings.ccc_beta * p0)
    report("  returned to (-beta p0, 0)", abs(p_new + settings.ccc_beta * p0) < tol and q_new < tol, f"p={p_new:.3f}")
    report("  softens: alpha up, p0 down", a > a0 and ccc_p0(a) < p0, f"p0 {p0:.1f} -> {ccc_p0(a):.1f}")

    # 7. Random sweep: output never violates the surface it was projected onto.
    worst, counts = -1e30, {0: 0, 1: 0, 2: 0, 3: 0}
    for _ in range(500):
        F = np.eye(3) + 0.02 * rng.standard_normal((3, 3))
        Fn, a, case, p0 = ccc_plasticity(F, a0)
        counts[case] += 1
        p, q = state_of(Fn)
        worst = max(worst, yield_value(p, q, p0))
    report("random sweep never outside the ellipse", worst < tol * tol,
           f"max y={worst:+.3e}  cases 0/1/2/3 = {counts[0]}/{counts[1]}/{counts[2]}/{counts[3]}")

    # 8. Repeated tension softens monotonically to fracture (p0 -> 0), with a brittle xi.
    #    The strain has to actually exceed the tensile strength: p < -beta p0 needs
    #    tr(eps) > beta p0 / K = 0.0077 here, i.e. more than 0.26 % per axis.
    settings = Settings(xi=1.0, p0=3000.0)
    a = ccc_initial_state()
    F_t = np.eye(3) * 1.01
    history = [ccc_p0(a)]
    for _ in range(200):
        _, a, _, _ = ccc_plasticity(F_t, a)
        history.append(ccc_p0(a))
    history = np.array(history)
    report("repeated tension: p0 monotone down to 0", np.all(np.diff(history) <= 1e-9) and history[-1] < 1e-6,
           f"p0 {history[0]:.1f} -> {history[-1]:.2e}")

    # 9. Repeated compression hardens monotonically. Needs p > p0: tr(eps) < -p0/K = -0.0386.
    a = ccc_initial_state()
    F_c = np.eye(3) * 0.98
    history = [ccc_p0(a)]
    for _ in range(50):
        _, a, _, _ = ccc_plasticity(F_c, a)
        history.append(ccc_p0(a))
    report("repeated compression: p0 monotone up", np.all(np.diff(history) >= -1e-9) and history[-1] > history[0],
           f"p0 {history[0]:.1f} -> {history[-1]:.1f}")

    # 10. beta = 0 is cohesionless: any tension at all is Case 2.
    settings = Settings(beta=0.0, xi=1.0)
    a = ccc_initial_state()
    _, _, case, _ = ccc_plasticity(np.eye(3) * 1.0001, a)
    report("beta = 0 has no tensile strength", case == 2)

    # 11. Li et al. 2021 Table 1: every case yields a finite initial state that round-trips.
    li = {"I cold dense": (0.5, 0.0, 1.0, 3e3), "II warm shear": (1.5, 0.3, 1.0, 30e3),
          "III sliding slab": (1.5, 0.5, 1.0, 42e3), "IV warm plug": (0.5, 1.0, 0.1, 12e3),
          "V verification": (0.7, 0.2, 0.002, 3e3)}
    all_fine = True
    for name, (M, b, xi, p0) in li.items():
        settings = Settings(E=3e6, nu=0.3, M=M, beta=b, xi=xi, p0=p0)
        a = ccc_initial_state()
        fine = math.isfinite(a) and abs(ccc_p0(a) - p0) < 1e-3
        all_fine &= fine
        print(f"      {name:18s} alpha0={a:+.5f}  p0={ccc_p0(a):.1f}  tensile={b * p0:.0f} Pa  {'ok' if fine else 'FAIL'}")
    report("Li 2021 Table 1 cases all round-trip", all_fine)

    # 12. Stress symmetry and rotation covariance (shared Hencky law).
    settings = Settings()
    F = np.array([[1.02, 0.01, 0], [0, 0.98, 0], [0, 0, 1.0]])
    R = svd3(rng.standard_normal((3, 3)))[0]
    tau, tau_r = ccc_stress(F), ccc_stress(R @ F)
    report("stress symmetric and rotates with F", np.allclose(tau, tau.T) and np.allclose(tau_r, R @ tau @ R.T, atol=tol))

    print("\nALL OK" if ok else "\nFAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
