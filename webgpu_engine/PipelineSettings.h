/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "compute/nodes/ComputeAvalancheTrajectoriesNode.h"
#include "compute/nodes/ComputeSnowNode.h"
#include "radix/geometry.h"
#include <QJsonObject>

namespace webgpu_engine {

// for preserving settings upon switching graph
// TODO quite ugly solution
struct ComputePipelineSettings {
    radix::geometry::Aabb<3, double> target_region = {}; // select tiles node
    uint32_t zoomlevel = 15;
    uint32_t trajectory_resolution_multiplier = 8;
    uint32_t num_steps = 10000u;
    float step_length = 0.1f;
    bool sync_snow_settings_with_render_settings = true; // snow node
    compute::nodes::ComputeSnowNode::SnowSettingsUniform snow_settings; // snow node

    compute::nodes::ComputeAvalancheTrajectoriesNode::PhysicsModelType model_type = compute::nodes::ComputeAvalancheTrajectoriesNode::PhysicsModelType::PHYSICS_SIMPLE;
    compute::nodes::ComputeAvalancheTrajectoriesNode::ModelPhysicsLessSimpleParams model_less_simple_params = {};

    int release_point_interval = 8; // trajectories node
    uint32_t num_paths_per_release_cell = 1024u;
    uint32_t num_runs = 1u;

    float random_contribution = 25.0f;
    float persistence_contribution = 0.9f;
    uint32_t random_seed = 1u;

    float trigger_point_min_slope_angle = 30.0f; // release points node
    float trigger_point_max_slope_angle = 45.0f; // release points node

    int tile_source_index = 0; // 0 DTM, 1 DSM

    int friction_model_type = int(compute::nodes::ComputeAvalancheTrajectoriesNode::FrictionModelType::VoellmyMinShear);
    compute::nodes::ComputeAvalancheTrajectoriesNode::RunoutPerlaParams perla;
    float runout_flowpy_alpha = 25.0f; // degrees

    // settings for buffer to texture
    glm::vec2 color_map_bounds = { 0.0f, 40.0f };
    glm::vec2 transparency_map_bounds = { 0.0f, 1.0f };
    bool use_bin_interpolation = false;
    bool use_transparency_buffer = true;
    bool texture_interpolation_mipmaps = false;

    // file paths for evaluation pipeline
    std::string release_points_texture_path;
    std::string heightmap_texture_path;
    std::string aabb_file_path;

    static void write_to_json_file(const ComputePipelineSettings& settings, const std::filesystem::path& output_path);
    static ComputePipelineSettings read_from_json_file(const std::filesystem::path& input_path);
};

} // namespace webgpu_engine
