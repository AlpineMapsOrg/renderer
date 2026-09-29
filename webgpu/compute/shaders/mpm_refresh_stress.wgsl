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
///use mpm_material

// Recomputes the cached Kirchhoff stress after a material edit that kept the particles.
//
// G2P stores tau = P F^T of the state it returned, and the next P2G uses it as is. That is
// only valid for the material parameters G2P ran with: after the stiffness or a plasticity
// parameter changed mid-run, the first P2G would still push with the old stress. This pass
// runs the return mapping once more on the stored elastic F under the current settings -
// for unchanged settings a no-op (the state is already admissible), otherwise the same
// projection the next G2P would apply - and stores the matching stress. Dispatched by the
// node only in the first chunk of a run whose stress-affecting settings changed.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    var p = particles[index];
    if p.mass <= 0.0 {
        return;
    }

    let plastic = material_plasticity(particle_f(p), p.plastic_state);
    store_f(&p, plastic.f_elastic);
    p.plastic_state = plastic.plastic_state;
    store_kirchhoff(&p, plastic.kirchhoff);
    particles[index] = p;
}
