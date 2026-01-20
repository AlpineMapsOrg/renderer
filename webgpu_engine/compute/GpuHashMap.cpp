/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "GpuHashMap.h"

#include "nucleus/srs.h"

namespace webgpu_engine::compute {

// explicit template specialization for hashing tile::Id and returning uint16_t hashes
template <> uint16_t gpu_hash<radix::tile::Id, uint16_t>(const radix::tile::Id& id) { return nucleus::srs::hash_uint16(id); }

} // namespace webgpu_engine::compute
