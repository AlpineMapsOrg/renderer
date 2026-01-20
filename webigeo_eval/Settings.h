/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Markus Rampp
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

namespace webigeo_eval {

struct Settings {
    // input and output paths
    std::string aabb_file_path;
    std::string release_cells_texture_path;
    std::string heightmap_texture_path;
    std::string output_dir_path;

    // hyper parameters
    uint32_t resolution_multiplier = 8u;
    uint32_t num_simulation_runs = 1u;
    uint32_t num_particles_per_release_cell = 1024u;
    uint32_t num_simulation_steps = 10000u;
    float simulation_step_length = 0.1f;
    uint32_t random_seed = 1u;

    // model parameters
    float max_random_deviation = 25.0f; // in degrees, also called theta
    float persistence = 0.9f; // in [0.0, 1.0]
    float max_runout_angle = 25.0f; // in degrees, also called alpha

    // other model parameters, experimental
    int model_type = 0;
    int friction_model_type = 0;
    float friction_coeff = .155f;
    float drag_coeff = 4000.0f;
    float slab_thickness = 0.5f;
    float density = 200.0f;

    static void write_to_json_file(const Settings& settings, const std::filesystem::path& output_path);
    static Settings read_from_json_file(const std::filesystem::path& input_path);
};

} // namespace webigeo_eval
