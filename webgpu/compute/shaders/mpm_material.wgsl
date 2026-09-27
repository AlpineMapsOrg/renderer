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
///use mpm_material_stomakhin
///use mpm_material_drucker_prager
///use mpm_material_ccc

// Constitutive model dispatcher.
//
// The MPM loop touches the material law in exactly two places, and every model has to
// provide these two functions:
//
//   <model>_initial_state()          plastic state a freshly seeded particle starts with
//   <model>_plasticity(F_trial, s)   return mapping after the elastic predictor in G2P; also
//                                    returns the Kirchhoff stress tau = P F^T of the result,
//                                    which the particle stores for the MLS-MPM force term of
//                                    the next P2G
//
// The stress used to be a third function evaluated in P2G, with its own SVD of F. Computing
// it at the end of the return mapping reuses that SVD - the elastic state after the return
// is U diag(sigma') V^T with the very same U and V - which halves the SVDs per substep.
//
// The active model is selected at runtime through settings.constitutive_model. The branch
// is on a uniform, so every particle takes the same path and there is no divergence cost -
// the same pattern ComputeAvalancheTrajectoriesNode uses for its physics models.
//
// Per-particle plastic state is one scalar for every model; what it means is up to the
// model (Stomakhin: plastic volume ratio Jp; Drucker-Prager: accumulated plastic strain;
// Cam-Clay: plastic volumetric strain alpha driving the consolidation pressure).
// The Lame parameters mu_0 / lambda_0 are shared by all models - one stiffness knob, with
// per-model recommended values supplied by presets rather than by duplicated settings.
//
// Adding a model: one new mpm_material_<name>.wgsl, a ///use above, a case in each of the
// two switches, an enum value in MpmSolverNode.h and a combo entry in the UI.

const MATERIAL_STOMAKHIN: u32 = 0u;
const MATERIAL_DRUCKER_PRAGER: u32 = 1u;
const MATERIAL_COHESIVE_CAM_CLAY: u32 = 2u;

fn material_initial_state() -> f32 {
    switch settings.constitutive_model {
        case MATERIAL_DRUCKER_PRAGER: {
            return dp_initial_state();
        }
        case MATERIAL_COHESIVE_CAM_CLAY: {
            return ccc_initial_state();
        }
        default: {
            return stomakhin_initial_state();
        }
    }
}

fn material_plasticity(f_trial: mat3x3f, plastic_state: f32) -> PlasticReturn {
    switch settings.constitutive_model {
        case MATERIAL_DRUCKER_PRAGER: {
            return dp_plasticity(f_trial, plastic_state);
        }
        case MATERIAL_COHESIVE_CAM_CLAY: {
            return ccc_plasticity(f_trial, plastic_state);
        }
        default: {
            return stomakhin_plasticity(f_trial, plastic_state);
        }
    }
}
