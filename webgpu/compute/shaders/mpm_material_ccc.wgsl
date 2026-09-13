/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

///use mpm_common

// Constitutive model: Cohesive Cam Clay - Gaume et al. 2018, "Dynamic anticrack propagation
// in snow" (yield surface, hardening law, parameters), with the three-case return mapping
// of Wolper et al. 2019 (NACC), the same group's implementation of this yield surface.
// Parameter values per flow regime come from Li et al. 2021, Table 1.
//
// Critical-state soil mechanics for a porous cohesive material. Hencky elasticity as in the
// Drucker-Prager model, but the yield surface is an ellipse in (p, q) space:
//
//   p  = -K tr(eps)                       mean pressure, positive in compression
//   q  = sqrt(3/2) * 2 mu |dev(eps)|      von Mises equivalent stress
//   y  = (1 + 2 beta) q^2 + M^2 (p + beta p0)(p - p0)  <= 0
//
// so the admissible pressure range is [-beta p0, p0]: beta p0 is the tensile strength,
// p0 the consolidation (compressive) strength, M the slope of the critical state line.
// This is what gives snow its mixed-mode failure - tensile, shear and compressive - in one
// surface.
//
// Hardening / softening in one law:  p0 = K sinh(xi * max(-alpha, 0)),  alpha the plastic
// volumetric strain. Compaction (alpha < 0) grows p0; dilation shrinks it towards zero,
// where the ellipse collapses to a point and the material carries no stress at all - that
// is fracture / granulation in this model.
//
// Return mapping, explicit (old p0 used for the projection, then hardening updated):
//   Case 1  p > p0            compressive cap:  -> (p0, 0), alpha decreases (harden)
//   Case 2  p < -beta p0      tensile tip:      -> (-beta p0, 0), alpha increases (soften)
//   Case 3  y > 0 otherwise   shear failure:    q projected onto the ellipse at fixed p
//   else                      elastic
// Gaume describes an associative flow rule; the fixed-p projection here is Wolper's
// non-associated variant. Swapping in an associative return only touches ccc_plasticity().
//
// Per-particle plastic state: alpha, initialised so that p0(alpha) = ccc_p0_initial.
// Parameters: settings.mu_0, lambda_0 (shared), ccc_m, ccc_beta, ccc_xi, ccc_p0_initial.

fn ccc_bulk_modulus() -> f32 { return settings.lambda_0 + 2.0 * settings.mu_0 / 3.0; }

fn ccc_asinh(x: f32) -> f32 { return log(x + sqrt(x * x + 1.0)); }

// Consolidation pressure from the plastic volumetric strain. The sinh argument is clamped
// so a runaway compaction cannot overflow the stress.
fn ccc_p0(alpha: f32) -> f32 {
    let arg = clamp(settings.ccc_xi * max(-alpha, 0.0), 0.0, 20.0);
    return ccc_bulk_modulus() * sinh(arg);
}

// alpha such that p0(alpha) equals the configured initial consolidation pressure.
fn ccc_initial_state() -> f32 {
    let k = ccc_bulk_modulus();
    return -ccc_asinh(settings.ccc_p0_initial / k) / max(settings.ccc_xi, 1e-6);
}

fn ccc_hencky_strain(sigma: vec3f) -> vec3f { return log(clamp(sigma, vec3f(1e-3), vec3f(1e3))); }

fn ccc_diag(v: vec3f) -> mat3x3f { return mat3x3f(vec3f(v.x, 0, 0), vec3f(0, v.y, 0), vec3f(0, 0, v.z)); }

// P F^T for Hencky elasticity - identical to the Drucker-Prager stress; only the yield
// surface differs between the two models.
fn ccc_stress(f_elastic: mat3x3f, alpha: f32) -> mat3x3f {
    let svd = svd3(f_elastic);
    let eps = ccc_hencky_strain(svd.sigma);
    let trace = eps.x + eps.y + eps.z;
    let tau = 2.0 * settings.mu_0 * eps + vec3f(settings.lambda_0 * trace);
    return svd.u * ccc_diag(tau) * transpose(svd.u);
}

fn ccc_plasticity(f_trial: mat3x3f, alpha: f32) -> PlasticReturn {
    let svd = svd3(f_trial);
    let eps = ccc_hencky_strain(svd.sigma);
    let trace = eps.x + eps.y + eps.z;
    let dev = eps - vec3f(trace / 3.0);
    let dev_norm = length(dev);

    let k = ccc_bulk_modulus();
    let mu = settings.mu_0;
    let m = settings.ccc_m;
    let beta = settings.ccc_beta;
    let p0 = ccc_p0(alpha);

    let p = -k * trace;
    let q = sqrt(1.5) * 2.0 * mu * dev_norm;

    var new_eps = eps;
    var new_alpha = alpha;

    if p > p0 {
        // Case 1: beyond the compressive cap. Return to (p0, 0): a purely volumetric strain
        // with tr = -p0/k, deviatoric part dropped. The removed volume is plastic compaction.
        new_eps = vec3f(-p0 / (3.0 * k));
        new_alpha = alpha + (trace + p0 / k);
    } else if p < -beta * p0 {
        // Case 2: beyond the tensile strength. Return to (-beta p0, 0); the removed
        // dilation softens the material.
        new_eps = vec3f(beta * p0 / (3.0 * k));
        new_alpha = alpha + (trace - beta * p0 / k);
    } else {
        let y = (1.0 + 2.0 * beta) * q * q + m * m * (p + beta * p0) * (p - p0);
        if y > 0.0 {
            // Case 3: shear failure. With p inside [-beta p0, p0] the ellipse term is <= 0,
            // so y > 0 implies q > 0 - the division is safe. Project q onto the ellipse at
            // fixed p; no volumetric plastic strain, so no hardening from shear alone.
            let q_new = m * sqrt(max((p + beta * p0) * (p0 - p) / (1.0 + 2.0 * beta), 0.0));
            new_eps = vec3f(trace / 3.0) + dev * (q_new / max(q, 1e-12));
        }
        // else: inside the yield surface, elastic.
    }

    var result: PlasticReturn;
    result.f_elastic = svd.u * ccc_diag(exp(new_eps)) * transpose(svd.v);
    result.plastic_state = new_alpha;
    return result;
}
