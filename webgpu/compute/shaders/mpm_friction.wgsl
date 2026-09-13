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

// Basal friction: the contact law between flowing snow and the terrain surface.
//
// Deliberately a separate module from the constitutive model. Internal friction (how snow
// shears against itself) belongs to the material law; basal friction is a boundary
// condition applied to velocities at the terrain. The two are easy to conflate and must
// not be - they are selected by independent settings.
//
// Two kinds of term live here and they are applied at different places:
//
//   * A contact impulse (Coulomb). Depends on the normal impact speed vn, so it is safe to
//     apply both at the grid node and again on the particle after advection - by the second
//     call vn is ~0 and the impulse vanishes.
//   * A velocity-dependent basal drag (the Voellmy turbulent term). Depends on |v_t|^2, not
//     on vn, so applying it at both levels would double-count it per substep. It is applied
//     at the grid level only, gated by the `apply_basal_drag` flag.

const FRICTION_COULOMB: u32 = 0u;
const FRICTION_VOELLMY: u32 = 1u;

// Coulomb: the tangential velocity loses mu times the normal impact speed per contact;
// if that would reverse it, the material sticks.
fn coulomb_friction(velocity: vec3f, normal: vec3f, vn: f32) -> vec3f {
    let vt = velocity - normal * vn;
    let vt_len = length(vt);
    if vt_len <= -settings.terrain_friction * vn {
        return vec3f(0.0); // sticking
    }
    return vt * (1.0 + settings.terrain_friction * vn / vt_len);
}

// Voellmy: Coulomb plus a turbulent drag quadratic in speed,
//     tau = mu * sigma_n + rho * g * |v|^2 / xi
// (Tonnel et al. 2023, com1DFA; same form as ComputeAvalancheTrajectoriesNode). Divided by
// rho * h to get a deceleration, with h = slab_thickness as the reference flow depth - the
// convention the trajectories node also uses (it hard-codes h = 1 m). Keeping the 1/h makes
// xi transferable from the literature: com1DFA's default is xi = 4000 m/s^2 paired with a
// lower mu of 0.155, since the drag term carries part of the resistance.
fn voellmy_friction(velocity: vec3f, normal: vec3f, vn: f32, apply_basal_drag: bool) -> vec3f {
    var vt = coulomb_friction(velocity, normal, vn);
    if !apply_basal_drag {
        return vt;
    }
    let speed = length(vt);
    if speed < 1e-6 {
        return vt;
    }
    let reference_depth = max(settings.slab_thickness, 0.1);
    let deceleration = settings.gravity * speed * speed / (settings.voellmy_xi * reference_depth);
    // Never let the drag reverse the flow.
    let new_speed = max(speed - deceleration * settings.dt, 0.0);
    return vt * (new_speed / speed);
}

// Resolves a velocity against the terrain; returns the corrected velocity.
// `apply_basal_drag` must be true for exactly one call site per substep (the grid update).
fn resolve_terrain_collision(velocity: vec3f, normal: vec3f, apply_basal_drag: bool) -> vec3f {
    let vn = dot(velocity, normal);
    if vn >= 0.0 {
        return velocity; // separating, nothing to do
    }
    switch settings.basal_friction_model {
        case FRICTION_VOELLMY: {
            return voellmy_friction(velocity, normal, vn, apply_basal_drag);
        }
        default: {
            return coulomb_friction(velocity, normal, vn);
        }
    }
}
