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

// Constitutive model dispatcher.
//
// The MPM loop touches the material law in exactly three places, and every model has to
// provide these three functions:
//
//   <model>_initial_state()          plastic state a freshly seeded particle starts with
//   <model>_stress(F, state)         P F^T, consumed by the MLS-MPM force term in P2G
//   <model>_plasticity(F_trial, s)   return mapping after the elastic predictor in G2P
//
// The active model is selected at runtime through settings.constitutive_model. The branch
// is on a uniform, so every particle takes the same path and there is no divergence cost -
// the same pattern ComputeAvalancheTrajectoriesNode uses for its physics models.
//
// Per-particle plastic state is one scalar for every model; what it means is up to the
// model (Stomakhin: plastic volume ratio Jp).
//
// Adding a model: one new mpm_material_<name>.wgsl, a ///use above, a case in each of the
// three switches, an enum value in MpmSolverNode.h and a combo entry in the UI.

const MATERIAL_STOMAKHIN: u32 = 0u;

fn material_initial_state() -> f32 {
    switch settings.constitutive_model {
        default: {
            return stomakhin_initial_state();
        }
    }
}

fn material_stress(f_elastic: mat3x3f, plastic_state: f32) -> mat3x3f {
    switch settings.constitutive_model {
        default: {
            return stomakhin_stress(f_elastic, plastic_state);
        }
    }
}

fn material_plasticity(f_trial: mat3x3f, plastic_state: f32) -> PlasticReturn {
    switch settings.constitutive_model {
        default: {
            return stomakhin_plasticity(f_trial, plastic_state);
        }
    }
}
