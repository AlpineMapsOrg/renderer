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

// Constitutive model: Klar et al. 2016, "Drucker-Prager elastoplasticity for sand animation".
//
// Hencky (logarithmic) strain hyperelasticity with a pressure-dependent friction cone as
// the yield surface and a closed-form projection back onto it. Cohesionless: it produces a
// dry granular flow with an angle of repose, but no slab, no fracture, no plug - in the
// language of Li et al. 2021 it covers the cold-dense regime only. Cheaper than Cam-Clay
// (a projection onto a cone versus an implicit return onto an ellipse), which is why it is
// the documented fallback if CCC proves too slow.
//
// Everything happens in the principal frame of F = U diag(sigma) V^T:
//   eps   = log(sigma)                              Hencky strain
//   tau   = 2 mu eps + lambda tr(eps)               Kirchhoff stress, principal values
//   yield = |dev(tau)| + alpha tr(tau) <= 0         Drucker-Prager cone
//
// Per-particle plastic state: accumulated plastic strain magnitude (sum of the projection
// distances). Not fed back into the model here - Klar's hardening of the friction angle is
// left out - but it is a free diagnostic of which particles have yielded.
// Parameters: settings.mu_0, lambda_0 (shared Lame parameters), dp_alpha (precomputed
// from the friction angle, see MpmSolverNode::update_gpu_settings).

fn dp_initial_state() -> f32 { return 0.0; }

// Hencky strain from singular values. The signed SVD can hand back a negative sigma for an
// inverted element; the clamp treats that as extreme compression instead of producing NaN.
fn dp_hencky_strain(sigma: vec3f) -> vec3f { return log(clamp(sigma, vec3f(1e-3), vec3f(1e3))); }

fn dp_diag(v: vec3f) -> mat3x3f { return mat3x3f(vec3f(v.x, 0, 0), vec3f(0, v.y, 0), vec3f(0, 0, v.z)); }

// P F^T = U diag(tau) U^T for the Hencky model - the Kirchhoff stress rotated back out of
// the principal frame.
fn dp_stress(f_elastic: mat3x3f, plastic_state: f32) -> mat3x3f {
    let svd = svd3(f_elastic);
    let eps = dp_hencky_strain(svd.sigma);
    let trace = eps.x + eps.y + eps.z;
    let tau = 2.0 * settings.mu_0 * eps + vec3f(settings.lambda_0 * trace);
    return svd.u * dp_diag(tau) * transpose(svd.u);
}

// Return mapping (Klar et al. 2016, section 5.3), on the Hencky strain in the principal frame.
fn dp_plasticity(f_trial: mat3x3f, plastic_state: f32) -> PlasticReturn {
    let svd = svd3(f_trial);
    let eps = dp_hencky_strain(svd.sigma);
    let trace = eps.x + eps.y + eps.z;
    let dev = eps - vec3f(trace / 3.0);
    let dev_norm = length(dev);

    var new_eps = eps;
    var delta_gamma = 0.0;

    if trace > 0.0 {
        // Case II: volumetric expansion. A cohesionless material cannot carry tension, and
        // the cone's apex is at zero stress - project to the tip, i.e. drop all elastic strain.
        new_eps = vec3f(0.0);
        delta_gamma = length(eps);
    } else {
        // Distance to the cone along the deviatoric direction. The (3 lambda + 2 mu)/(2 mu)
        // factor converts the trace of strain into the trace of stress. With trace <= 0 the
        // second term is <= 0, so delta_gamma > 0 implies dev_norm > 0 - no division guard
        // needed, and pure hydrostatic compression (dev = 0) correctly stays elastic.
        delta_gamma = dev_norm + (3.0 * settings.lambda_0 + 2.0 * settings.mu_0) / (2.0 * settings.mu_0) * trace * settings.dp_alpha;
        if delta_gamma <= 0.0 {
            // Case I: inside the yield surface, purely elastic.
            delta_gamma = 0.0;
        } else {
            // Case III: project onto the cone surface.
            new_eps = eps - delta_gamma * dev / dev_norm;
        }
    }

    var result: PlasticReturn;
    result.f_elastic = svd.u * dp_diag(exp(new_eps)) * transpose(svd.v);
    result.plastic_state = plastic_state + delta_gamma;
    return result;
}
