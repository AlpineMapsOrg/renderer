#!/usr/bin/env python3
"""Verbatim port of svd3() from mpm_common.wgsl, checked against numpy.

The 3x3 SVD is the highest-risk part of the solver: a bug there produces plausible-looking
garbage rather than an obvious failure. WGSL matrices are column-major and indexed
m[col][row]; numpy is A[row, col]. The port keeps the same scalar formulas and the same
rotation order, so a bug here is a bug in the shader.

    python3 test_svd.py
"""
import numpy as np


def svd3(F):
    """F = U diag(sigma) V^T, det(U) = det(V) = +1, sigma[2] signed."""
    A = F.T @ F
    a00, a01, a02 = A[0, 0], A[0, 1], A[0, 2]
    a11, a12 = A[1, 1], A[1, 2]
    a22 = A[2, 2]
    V = np.eye(3)

    # Cyclic Jacobi eigendecomposition of the symmetric matrix A.
    for _ in range(8):
        # (0,1) plane, spectator 2
        if abs(a01) > 1e-12:
            theta = (a11 - a00) / (2.0 * a01)
            t = 1.0 / (abs(theta) + np.sqrt(theta * theta + 1.0))
            if theta < 0.0:
                t = -t
            c = 1.0 / np.sqrt(t * t + 1.0)
            s = t * c
            n02 = c * a02 - s * a12
            n12 = s * a02 + c * a12
            a00, a11 = a00 - t * a01, a11 + t * a01
            a01, a02, a12 = 0.0, n02, n12
            v0, v1 = V[:, 0].copy(), V[:, 1].copy()
            V[:, 0], V[:, 1] = c * v0 - s * v1, s * v0 + c * v1
        # (0,2) plane, spectator 1
        if abs(a02) > 1e-12:
            theta = (a22 - a00) / (2.0 * a02)
            t = 1.0 / (abs(theta) + np.sqrt(theta * theta + 1.0))
            if theta < 0.0:
                t = -t
            c = 1.0 / np.sqrt(t * t + 1.0)
            s = t * c
            n01 = c * a01 - s * a12
            n12 = s * a01 + c * a12
            a00, a22 = a00 - t * a02, a22 + t * a02
            a02, a01, a12 = 0.0, n01, n12
            v0, v2 = V[:, 0].copy(), V[:, 2].copy()
            V[:, 0], V[:, 2] = c * v0 - s * v2, s * v0 + c * v2
        # (1,2) plane, spectator 0
        if abs(a12) > 1e-12:
            theta = (a22 - a11) / (2.0 * a12)
            t = 1.0 / (abs(theta) + np.sqrt(theta * theta + 1.0))
            if theta < 0.0:
                t = -t
            c = 1.0 / np.sqrt(t * t + 1.0)
            s = t * c
            n01 = c * a01 - s * a02
            n02 = s * a01 + c * a02
            a11, a22 = a11 - t * a12, a22 + t * a12
            a12, a01, a02 = 0.0, n01, n02
            v1, v2 = V[:, 1].copy(), V[:, 2].copy()
            V[:, 1], V[:, 2] = c * v1 - s * v2, s * v1 + c * v2

    # Sort eigenvalues (and eigenvectors) descending.
    vals = [a00, a11, a22]
    vecs = [V[:, 0].copy(), V[:, 1].copy(), V[:, 2].copy()]
    for i in range(2):
        for j in range(2 - i):
            if vals[j] < vals[j + 1]:
                vals[j], vals[j + 1] = vals[j + 1], vals[j]
                vecs[j], vecs[j + 1] = vecs[j + 1], vecs[j]

    v0, v1, v2 = vecs
    if np.linalg.det(np.column_stack([v0, v1, v2])) < 0.0:
        v2 = -v2  # keep V a proper rotation

    sigma0 = np.sqrt(max(vals[0], 0.0))
    sigma1 = np.sqrt(max(vals[1], 0.0))
    fv0, fv1, fv2 = F @ v0, F @ v1, F @ v2

    u0 = fv0 / sigma0 if sigma0 > 1e-9 else np.array([1.0, 0, 0])
    n = np.linalg.norm(u0)
    u0 = u0 / n if n > 1e-9 else np.array([1.0, 0, 0])

    u1 = fv1 / sigma1 if sigma1 > 1e-9 else fv1
    u1 = u1 - u0 * np.dot(u0, u1)  # re-orthogonalise
    n = np.linalg.norm(u1)
    if n > 1e-9:
        u1 = u1 / n
    else:
        helper = np.array([0.0, 1, 0]) if abs(u0[0]) > 0.9 else np.array([1.0, 0, 0])
        u1 = np.cross(u0, helper) / np.linalg.norm(np.cross(u0, helper))

    u2 = np.cross(u0, u1)
    sigma2 = np.dot(u2, fv2)  # signed: reproduces det(F) < 0

    return np.column_stack([u0, u1, u2]), np.array([sigma0, sigma1, sigma2]), \
        np.column_stack([v0, v1, v2])


def check(F, label, quiet=False):
    U, S, V = svd3(F)
    err = np.abs(U @ np.diag(S) @ V.T - F).max()
    orth_u = np.abs(U.T @ U - np.eye(3)).max()
    orth_v = np.abs(V.T @ V - np.eye(3)).max()
    ok = err < 1e-4 and orth_u < 1e-5 and orth_v < 1e-5
    if not quiet:
        print(f"{label:28s} recon_err={err:.2e} orthU={orth_u:.1e} orthV={orth_v:.1e} "
              f"detU={np.linalg.det(U):+.3f} detV={np.linalg.det(V):+.3f} "
              f"sign(detF)={np.sign(np.linalg.det(F)):+.0f} {'OK' if ok else 'FAIL'}")
    elif not ok:
        print(f"{label} FAILED err={err:.2e}")
    return ok


def main():
    rng = np.random.default_rng(7)
    ok = True

    ok &= check(np.eye(3), "identity")
    ok &= check(np.diag([0.5, 0.5, 0.5]), "uniform compression")
    ok &= check(np.diag([2.0, 0.5, 1.0]), "anisotropic")
    ok &= check(np.diag([1.0, 1.0, -1.0]), "inverted (det<0)")
    ok &= check(np.diag([1.0, 1.0, 1e-7]), "near-degenerate")
    ok &= check(np.array([[0.0, -1, 0], [1.0, 0, 0], [0, 0, 1.0]]), "pure rotation")

    # Typical MPM deformation gradients stay near identity.
    for i in range(200):
        ok &= check(np.eye(3) + 0.15 * rng.standard_normal((3, 3)),
                    f"near-identity #{i}", quiet=True)
    # Stress the algorithm well outside its normal operating range.
    for i in range(500):
        ok &= check(rng.standard_normal((3, 3)), f"random #{i}", quiet=True)

    print("\n700+ random matrices checked.")
    print("ALL OK" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
