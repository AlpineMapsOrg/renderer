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

// Constitutive model: Stomakhin et al. 2013, "A material point method for snow simulation".
//
// Elastoplastic with a multiplicative split F = Fe Fp. The elastic part is the fixed
// corotated model; plasticity is a box clamp on the singular values of Fe against critical
// compression / stretch thresholds; compaction hardening is exponential in the plastic
// volume ratio.
//
// Per-particle plastic state: Jp, the accumulated plastic volume change. 1 = pristine.
// Parameters: settings.mu_0, lambda_0, hardening (xi), critical_compression (theta_c),
// critical_stretch (theta_s).

fn stomakhin_initial_state() -> f32 { return 1.0; }

// First Piola-Kirchhoff stress premultiplied by F^T, as required by the MLS-MPM force term.
fn stomakhin_stress(f_elastic: mat3x3f, jp: f32) -> mat3x3f {
    let svd = svd3(f_elastic);

    // Hardening: compacted snow (jp < 1) becomes stiffer.
    let h = clamp(exp(settings.hardening * (1.0 - jp)), 0.05, 20.0);
    let mu = settings.mu_0 * h;
    let lambda = settings.lambda_0 * h;

    let r = svd.u * transpose(svd.v); // rotational part of F
    let j = svd.sigma.x * svd.sigma.y * svd.sigma.z;

    let deviatoric = 2.0 * mu * (f_elastic - r) * transpose(f_elastic);
    let volumetric = lambda * j * (j - 1.0);
    return deviatoric + identity3() * volumetric;
}

// Push the elastic deformation gradient back into the admissible range and move the
// removed part into the plastic state jp.
fn stomakhin_plasticity(f_trial: mat3x3f, jp: f32) -> PlasticReturn {
    let svd = svd3(f_trial);

    let lo = 1.0 - settings.critical_compression;
    let hi = 1.0 + settings.critical_stretch;
    let clamped = clamp(svd.sigma, vec3f(lo), vec3f(hi));

    // jp accumulates the volume change that was clamped away.
    let ratio = (svd.sigma.x / clamped.x) * (svd.sigma.y / clamped.y) * (svd.sigma.z / clamped.z);

    var result: PlasticReturn;
    result.plastic_state = clamp(jp * ratio, 0.05, 20.0);
    result.f_elastic = svd.u * mat3x3f(vec3f(clamped.x, 0, 0), vec3f(0, clamped.y, 0), vec3f(0, 0, clamped.z)) * transpose(svd.v);
    return result;
}
