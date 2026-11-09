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

// The baseline record of a vector field: one per cell of a regular 2D grid, 32 bytes. Every
// avalanche model that delivers a flow field fills this record, and everything that reads one
// (a streamline renderer, an export) depends on this file and on VectorField.h, never on a
// model's own layout. Binding-free: the including shader declares the buffer.
//
// The first eight words are fixed by agreement; a field may carry a larger record whose first
// eight words are this one (VectorField::stride_words). Any change here bumps
// VECTOR_FIELD_VERSION, which is VECTOR_FIELD_VERSION in VectorField.h.
//
// Frame: the field's own metre frame, the one its positions are in, with z up. The velocity
// is in that frame; VectorField::world_from_metres places the frame on the map.

const VECTOR_FIELD_VERSION: u32 = 1u;
const VECTOR_FIELD_STRIDE_WORDS: u32 = 8u;

// Which channels of a record are valid. A cell without flow has flags 0 and all channels 0.
const FIELD_VELOCITY: u32 = 1u; // velocity
const FIELD_DEPTH: u32 = 2u; // depth
const FIELD_MASS: u32 = 4u; // mass
const FIELD_PRESSURE: u32 = 8u; // pressure
const FIELD_COHERENCE: u32 = 16u; // coherence

struct FieldRecord {
    velocity: vec3f, // [m/s] mean flow velocity of the cell: for a depth-resolved model the mass-weighted mean over depth
    depth: f32, // [m] flow depth (vertical thickness of the snow)
    mass: f32, // [kg] snow in the cell
    pressure: f32, // [Pa] basal normal stress
    coherence: f32, // 0..1, how well the directions in the cell agree (1 = all the same); models of paths, not of a continuum
    flags: u32, // FIELD_* bits
}
