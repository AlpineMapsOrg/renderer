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

#include <WebigeoApp.h>

#include <QEventLoop>
#include <chrono>
#include <thread>

#include <webgpu_engine/compute/nodes/BufferExportNode.h>
#include <webgpu_engine/compute/nodes/BufferToTextureNode.h>
#include <webgpu_engine/compute/nodes/ComputeAvalancheTrajectoriesNode.h>
#include <webgpu_engine/compute/nodes/LoadRegionAabbNode.h>
#include <webgpu_engine/compute/nodes/LoadTextureNode.h>
#include <webgpu_engine/compute/nodes/TileExportNode.h>
#include <webgpu_engine/compute/nodes/util.h>

#include "Settings.h"
#include "util/webgpu_init.h"

namespace webigeo_eval {

WebigeoApp::WebigeoApp()
    : m_webgpu_instance { util::init_webgpu_instance() }
    , m_webgpu_adapter { util::init_webgpu_adapter(m_webgpu_instance) }
    , m_device { util::init_webgpu_device(m_webgpu_instance, m_webgpu_adapter) }
    , m_context { std::make_unique<webgpu_engine::Context>() }
{
    m_context->set_webgpu_device(m_device);
    m_context->initialise();

    m_node_graph = webgpu_engine::compute::nodes::NodeGraph::create_trajectories_evaluation_compute_graph(*m_context->pipeline_manager(), m_device);
    connect(m_node_graph.get(), &webgpu_engine::compute::nodes::NodeGraph::run_completed, this, &WebigeoApp::run_completed);
    connect(m_node_graph.get(), &webgpu_engine::compute::nodes::NodeGraph::run_failed, this, &WebigeoApp::run_failed);
}

WebigeoApp::~WebigeoApp() { m_context->destroy(); }

void WebigeoApp::run()
{
    m_node_graph->run();

    while (!m_run_ended) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        wgpuInstanceProcessEvents(m_webgpu_instance); // needed to receive buffer/texture readback callbacks
    }

    // write settings and timings
    std::filesystem::path output_dir_path = m_settings.output_dir_path;
    std::filesystem::create_directories(output_dir_path);
    std::filesystem::path settings_export_path = output_dir_path / "settings.json";
    qDebug() << "writing settings to " << settings_export_path.string();
    Settings::write_to_json_file(m_settings, settings_export_path);
    std::filesystem::path timings_export_path = output_dir_path / "timings.json";
    qDebug() << "writing timings to " << timings_export_path.string();
    webgpu_engine::compute::nodes::write_timings_to_json_file(*m_node_graph, timings_export_path);
}

void WebigeoApp::run_completed()
{
    qInfo() << "run successful";
    m_run_ended = true;
}

void WebigeoApp::run_failed(webgpu_engine::compute::nodes::GraphRunFailureInfo info)
{
    // print error message and exit
    qFatal() << "node" << info.node_name() << "failed: " << info.node_run_failure_info().message();
    m_run_ended = true;
}

void WebigeoApp::update_settings(const Settings& node_graph_settings)
{
    using namespace webgpu_engine::compute::nodes;

    m_settings = node_graph_settings;

    // trajectories settings
    ComputeAvalancheTrajectoriesNode::AvalancheTrajectoriesSettings trajectory_settings {};
    trajectory_settings.resolution_multiplier = node_graph_settings.resolution_multiplier;
    trajectory_settings.num_runs = node_graph_settings.num_simulation_runs;
    trajectory_settings.num_paths_per_release_cell = node_graph_settings.num_particles_per_release_cell;
    trajectory_settings.num_steps = node_graph_settings.num_simulation_steps;
    trajectory_settings.step_length = node_graph_settings.simulation_step_length;
    trajectory_settings.random_seed = node_graph_settings.random_seed;

    trajectory_settings.random_contribution = node_graph_settings.max_random_deviation;
    trajectory_settings.persistence_contribution = node_graph_settings.persistence;

    trajectory_settings.active_runout_model = static_cast<ComputeAvalancheTrajectoriesNode::FrictionModelType>(node_graph_settings.friction_model_type);
    trajectory_settings.runout_flowpy = {};
    trajectory_settings.runout_flowpy.alpha = glm::radians(node_graph_settings.max_runout_angle);
    trajectory_settings.runout_perla = {};

    trajectory_settings.active_model = static_cast<ComputeAvalancheTrajectoriesNode::PhysicsModelType>(node_graph_settings.model_type);
    trajectory_settings.model2.friction_coeff = node_graph_settings.friction_coeff;
    trajectory_settings.model2.drag_coeff = node_graph_settings.drag_coeff;
    // trajectory_settings.model2.gravity = node_graph_settings.slab_thickness;
    // trajectory_settings.model2.mass = node_graph_settings.density;

    auto& trajectories_node = m_node_graph->get_node_as<ComputeAvalancheTrajectoriesNode>("compute_avalanche_trajectories_node");
    trajectories_node.set_settings(trajectory_settings);
    {
        BufferToTextureNode& node = m_node_graph->get_node_as<BufferToTextureNode>("buffer_to_texture_node");
        BufferToTextureNode::BufferToTextureSettings& settings = node.settings();
        settings.color_map_bounds = { 0.0f, 40.0f };
        settings.transparency_map_bounds = { 0.0f, 1.0f };
        settings.use_bin_interpolation = false;
        settings.use_transparency_buffer = true;
        settings.texture_filter_mode = WGPUFilterMode_Nearest;
        settings.texture_mipmap_filter_mode = WGPUMipmapFilterMode_Nearest;
        settings.texture_max_aniostropy = 1;
        settings.create_mipmaps = false;
    }

    {
        LoadTextureNode::LoadTextureNodeSettings settings;
        settings.format = WGPUTextureFormat_RGBA8Unorm;
        settings.file_path = node_graph_settings.release_cells_texture_path;
        m_node_graph->get_node_as<LoadTextureNode>("load_rp_node").set_settings(settings);
    }

    {
        LoadTextureNode::LoadTextureNodeSettings settings;
        settings.file_path = node_graph_settings.heightmap_texture_path;
        m_node_graph->get_node_as<LoadTextureNode>("load_heights_node").set_settings(settings);
    }

    {
        LoadRegionAabbNode::LoadRegionAabbNodeSettings settings;
        settings.file_path = node_graph_settings.aabb_file_path;
        m_node_graph->get_node_as<LoadRegionAabbNode>("load_aabb_node").set_settings(settings);
    }

    // update file export path to include current date and time
    {
        // root dir of all file exports
        const std::filesystem::path export_root_dir = node_graph_settings.output_dir_path;

        // set trajectory layer export directories
        BufferExportNode::ExportSettings zdelta_export_settings { (export_root_dir / "trajectories/texture_layer1_zdelta.png").string() };
        m_node_graph->get_node_as<BufferExportNode>("l1_export_node").set_settings(zdelta_export_settings);
        BufferExportNode::ExportSettings cell_counts_export_settings { (export_root_dir / "trajectories/texture_layer2_cellCounts.png").string() };
        m_node_graph->get_node_as<BufferExportNode>("l2_export_node").set_settings(cell_counts_export_settings);
        BufferExportNode::ExportSettings travel_length_export_settings { (export_root_dir / "trajectories/texture_layer3_travelLength.png").string() };
        m_node_graph->get_node_as<BufferExportNode>("l3_export_node").set_settings(travel_length_export_settings);
        BufferExportNode::ExportSettings travel_angle_export_settings { (export_root_dir / "trajectories/texture_layer4_travelAngle.png").string() };
        m_node_graph->get_node_as<BufferExportNode>("l4_export_node").set_settings(travel_angle_export_settings);
        BufferExportNode::ExportSettings height_diff_export_settings { (export_root_dir / "trajectories/texture_layer5_heightDifference.png").string() };
        m_node_graph->get_node_as<BufferExportNode>("l5_export_node").set_settings(height_diff_export_settings);

        // set trajectory color buffer export directory
        TileExportNode::ExportSettings export_trajectory_settings = { true, true, true, true, (export_root_dir / "trajectories").string() };
        m_node_graph->get_node_as<TileExportNode>("trajectories_export").set_settings(export_trajectory_settings);

        // set heightmap export directory
        TileExportNode::ExportSettings export_height_settings = { true, true, true, true, (export_root_dir / "heights").string() };
        m_node_graph->get_node_as<TileExportNode>("height_export").set_settings(export_height_settings);

        // set release point export directory
        TileExportNode::ExportSettings export_releasepoints_settings = { true, true, true, true, (export_root_dir / "release_points").string() };
        m_node_graph->get_node_as<TileExportNode>("rp_export").set_settings(export_releasepoints_settings);

        // set normals export directory
        TileExportNode::ExportSettings export_normals_settings = { true, true, true, true, (export_root_dir / "normals").string() };
        m_node_graph->get_node_as<TileExportNode>("normals_export").set_settings(export_normals_settings);
    }
}

} // namespace webigeo_eval
