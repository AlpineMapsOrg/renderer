/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2025 Patrick Komon
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

#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <nucleus/Raster.h>
#include <radix/geometry.h>
#include <string>
#include <tl/expected.hpp>
#include <vector>

class QString;

namespace nucleus::utils::geopng {

// Encoding range for the u32 linear float-in-RGBA encoding.
inline constexpr float ENCODED_FLOAT_RANGE_MIN = -10000.0f;
inline constexpr float ENCODED_FLOAT_RANGE_MAX = 10000.0f;

// Returns candidate AABB .txt file paths for the given geo-PNG image path.
// Tries in order:
//   - %filename%_aabb.txt
//   - %filename before first _%_aabb.txt
//   - aabb.txt
std::vector<std::filesystem::path> possible_aabb_paths(const std::filesystem::path& image_path);

// Parses a sidecar AABB .txt file describing the world-space extent of a geo-PNG.
// The file contains exactly four lines: min_x, min_y, max_x, max_y
// Returns the parsed AABB, or an error message on failure
tl::expected<radix::geometry::Aabb<2, double>, std::string> load_aabb_from_file(const std::filesystem::path& file_path);

// Encodes a Raster<float> as a geo-PNG: each float is clamped to
// [ENCODED_FLOAT_RANGE_MIN, ENCODED_FLOAT_RANGE_MAX], mapped to [0,1], packed
// as a u32, and stored across the R,G,B,A channels of a u8 PNG.
void write_encoded_float_png(const Raster<float>& data, const QString& filename);

// Scans an RGBA-encoded float image. Returns {min, max} of decoded values.
// Determines whether its likely_encoded_float with the heuristic that either:
//  - >= 1% of decoded float values are approx. 0.0
//  - all decoded values share the same sign
glm::vec2 scan_encoded_float_range(const Raster<glm::u8vec4>& image, bool& likely_encoded_float);

} // namespace nucleus::utils::geopng
