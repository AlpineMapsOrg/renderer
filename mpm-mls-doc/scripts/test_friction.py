#!/usr/bin/env python3
"""Verbatim port of mpm_friction.wgsl, checked against closed-form slope mechanics.

A point mass on an inclined plane, stepped exactly the way mpm_grid_update does it:
gravity first, then the terrain contact response. No grid, no particles - this isolates
the friction law.

Coulomb on a slope steeper than atan(mu) accelerates without bound; Voellmy's quadratic
drag gives a terminal velocity
    v_inf = sqrt(xi * h * (sin(theta) - mu * cos(theta)))
which is the property worth checking. Also checks that Coulomb sticks below atan(mu), and
that the drag can never reverse the flow.

    python3 test_friction.py
"""
import math

import numpy as np

G = 9.81


class Settings:
    terrain_friction = 0.155
    voellmy_xi = 4000.0
    slab_thickness = 1.5
    gravity = G
    dt = 0.01
    basal_friction_model = 1  # 0 Coulomb, 1 Voellmy


settings = Settings()


# --- port of mpm_friction.wgsl -------------------------------------------------------

def coulomb_friction(velocity, normal, vn):
    vt = velocity - normal * vn
    vt_len = np.linalg.norm(vt)
    if vt_len <= -settings.terrain_friction * vn:
        return np.zeros(3)  # sticking
    return vt * (1.0 + settings.terrain_friction * vn / vt_len)


def voellmy_friction(velocity, normal, vn, apply_basal_drag):
    vt = coulomb_friction(velocity, normal, vn)
    if not apply_basal_drag:
        return vt
    speed = np.linalg.norm(vt)
    if speed < 1e-6:
        return vt
    reference_depth = max(settings.slab_thickness, 0.1)
    deceleration = settings.gravity * speed * speed / (settings.voellmy_xi * reference_depth)
    new_speed = max(speed - deceleration * settings.dt, 0.0)
    return vt * (new_speed / speed)


def resolve_terrain_collision(velocity, normal, apply_basal_drag):
    vn = float(np.dot(velocity, normal))
    if vn >= 0.0:
        return velocity
    if settings.basal_friction_model == 1:
        return voellmy_friction(velocity, normal, vn, apply_basal_drag)
    return coulomb_friction(velocity, normal, vn)


# --- slope test bench ------------------------------------------------------------------

def run_slope(theta_deg, model, mu, steps=20000):
    """Point on a plane inclined by theta about the y axis, stepped like the grid update."""
    settings.basal_friction_model = model
    settings.terrain_friction = mu
    theta = math.radians(theta_deg)
    normal = np.array([-math.sin(theta), 0.0, math.cos(theta)])  # upward-facing
    v = np.zeros(3)
    history = []
    for _ in range(steps):
        v = v + np.array([0.0, 0.0, -G * settings.dt])          # gravity, as in mpm_grid_update
        v = resolve_terrain_collision(v, normal, True)             # then the contact response
        history.append(np.linalg.norm(v))
    return np.array(history)


def main():
    ok = True
    mu, xi, h = 0.155, 4000.0, 1.5
    settings.voellmy_xi, settings.slab_thickness = xi, h

    # 1. Voellmy terminal velocity on a 35 degree slope.
    theta = 35.0
    v_inf = math.sqrt(xi * h * (math.sin(math.radians(theta)) - mu * math.cos(math.radians(theta))))
    hist = run_slope(theta, model=1, mu=mu)
    v_final = hist[-1]
    # Explicit stepping lags the continuous solution by O(dt); 1% is the right tolerance.
    err = abs(v_final - v_inf) / v_inf
    print(f"Voellmy 35deg: terminal {v_final:.2f} m/s  analytic {v_inf:.2f} m/s  rel err {err:.3%}")
    if err > 0.01:
        print("  FAIL: terminal velocity off"); ok = False
    if np.any(np.diff(hist) < -1e-9):
        print("  FAIL: speed decreased while approaching terminal velocity"); ok = False
    if np.any(hist < 0):
        print("  FAIL: negative speed"); ok = False

    # 2. Coulomb on the same slope never settles - it must keep accelerating.
    hist_c = run_slope(theta, model=0, mu=mu)
    a_expected = G * (math.sin(math.radians(theta)) - mu * math.cos(math.radians(theta)))
    a_measured = (hist_c[-1] - hist_c[-1001]) / (1000 * settings.dt)
    print(f"Coulomb 35deg: acceleration {a_measured:.3f} m/s^2  analytic {a_expected:.3f} m/s^2")
    if abs(a_measured - a_expected) / a_expected > 0.01:
        print("  FAIL: Coulomb slope acceleration off"); ok = False

    # 3. Below the friction angle both models stick.
    theta_stick = math.degrees(math.atan(mu)) - 2.0
    for model, name in ((0, "Coulomb"), (1, "Voellmy")):
        hist_s = run_slope(theta_stick, model=model, mu=mu, steps=500)
        print(f"{name} {theta_stick:.1f}deg (below atan mu): final speed {hist_s[-1]:.2e} m/s")
        if hist_s[-1] > 1e-6:
            print(f"  FAIL: {name} should stick below the friction angle"); ok = False

    # 4. Drag is applied at the grid level only - the particle-level call must be pure Coulomb.
    settings.basal_friction_model = 1
    n = np.array([0.0, 0.0, 1.0])
    v = np.array([30.0, 0.0, -0.1])
    grid = resolve_terrain_collision(v, n, True)
    particle = resolve_terrain_collision(v, n, False)
    coulomb_only = coulomb_friction(v, n, float(np.dot(v, n)))
    print(f"flag: grid-level speed {np.linalg.norm(grid):.4f}, particle-level {np.linalg.norm(particle):.4f}, "
          f"pure Coulomb {np.linalg.norm(coulomb_only):.4f}")
    if not np.allclose(particle, coulomb_only):
        print("  FAIL: apply_basal_drag=false must reduce to Coulomb"); ok = False
    if np.linalg.norm(grid) >= np.linalg.norm(particle):
        print("  FAIL: grid-level drag should remove additional speed"); ok = False

    # 5. Drag never reverses the flow, even at absurd speed and dt.
    settings.dt = 1.0
    v = np.array([500.0, 0.0, -1.0])
    out = resolve_terrain_collision(v, n, True)
    print(f"reversal guard: 500 m/s with dt=1 s -> {out[0]:.3f} m/s (must be >= 0)")
    if out[0] < 0:
        print("  FAIL: drag reversed the flow"); ok = False
    settings.dt = 0.01

    print("\nALL OK" if ok else "\nFAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
