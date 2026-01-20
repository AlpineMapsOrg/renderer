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

#include "DownsampleTilesNode.h"

#include <QDebug>
#include <unordered_set>

namespace webgpu_engine::compute::nodes {

glm::uvec3 DownsampleTilesNode::SHADER_WORKGROUP_SIZE = { 1, 16, 16 };

DownsampleTilesNode::DownsampleTilesNode(const PipelineManager& pipeline_manager, WGPUDevice device, size_t capacity)
    : DownsampleTilesNode(pipeline_manager, device, capacity, DownsampleSettings())
{
}

DownsampleTilesNode::DownsampleTilesNode(const PipelineManager& pipeline_manager, WGPUDevice device, size_t capacity, const DownsampleSettings& settings)
    : Node(
          {
              InputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>()),
              InputSocket(*this, "hash map", data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()),
              InputSocket(*this, "textures", data_type<TileStorageTexture*>()),
          },
          {
              OutputSocket(*this, "hash map", data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>(), [this]() { return input_socket("hash map").connected_socket().get_data(); }),
              OutputSocket(*this, "textures", data_type<TileStorageTexture*>(), [this]() { return input_socket("textures").connected_socket().get_data(); }),
          })
    , m_pipeline_manager { &pipeline_manager }
    , m_device { device }
    , m_queue { wgpuDeviceGetQueue(m_device) }
    , m_settings { settings }
    , m_input_tile_ids(device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, capacity, "compute: downsampling, tile id buffer")
    , m_internal_storage_texture()
{
}

GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& DownsampleTilesNode::hash_map()
{
    return *std::get<data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()>(input_socket("hash map").get_connected_data());
}

TileStorageTexture& DownsampleTilesNode::texture_storage()
{
    return *std::get<data_type<TileStorageTexture*>()>(input_socket("textures").get_connected_data());
}

void DownsampleTilesNode::set_downsample_settings(const DownsampleSettings& settings) { m_settings = settings; }

const DownsampleTilesNode::DownsampleSettings& DownsampleTilesNode::get_downsample_settings() const { return m_settings; }

void DownsampleTilesNode::run_impl()
{
    qDebug() << "running DownsampleTilesNode ...";
    qDebug() << "downsampling over " << m_settings.num_levels << " zoom levels";
    if (m_settings.num_levels == 0) {
        qDebug() << "nothing to do, emit completed signal immediately";
        emit run_completed();
        return;
    }

    const auto& original_tile_ids = *std::get<data_type<const std::vector<radix::tile::Id>*>()>(input_socket("tile ids").get_connected_data());
    auto& hash_map = *std::get<data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()>(input_socket("hash map").get_connected_data());
    auto& hashmap_textures = *std::get<data_type<TileStorageTexture*>()>(input_socket("textures").get_connected_data());

    // determine downsampled tile ids
    std::vector<radix::tile::Id> downsampled_tile_ids = get_tile_ids_for_downsampled_tiles(original_tile_ids);

    // (re)create storage texture to write downsampled tiles to
    m_internal_storage_texture = std::make_unique<TileStorageTexture>(m_device, glm::uvec2 { hashmap_textures.width(), hashmap_textures.height() },
        downsampled_tile_ids.size(), hashmap_textures.texture().texture().descriptor().format, WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc);

    // (re)create bind group
    // TODO re-create bind groups only when input handles change
    std::vector<WGPUBindGroupEntry> entries {
        m_input_tile_ids.create_bind_group_entry(0),
        hash_map.key_buffer().create_bind_group_entry(1),
        hash_map.value_buffer().create_bind_group_entry(2),
        hashmap_textures.texture().texture_view().create_bind_group_entry(3),
        m_internal_storage_texture->texture().texture_view().create_bind_group_entry(4),
    };
    m_compute_bind_group = std::make_unique<webgpu::raii::BindGroup>(
        m_device, m_pipeline_manager->downsample_compute_bind_group_layout(), entries, "compute: downsample bind group");

    // TODO smarter way of doing this, less duplicated code - maybe refactor compute_downsampled_tiles
    std::optional<NodeRunFailureInfo> potential_failure = compute_downsampled_tiles(downsampled_tile_ids);
    if (potential_failure.has_value()) {
        emit run_failed(potential_failure.value());
        return;
    }

    for (size_t i = 1; i < m_settings.num_levels; i++) {
        downsampled_tile_ids = get_tile_ids_for_downsampled_tiles(downsampled_tile_ids);
        std::optional<NodeRunFailureInfo> potential_failure2 = compute_downsampled_tiles(downsampled_tile_ids);
        if (potential_failure2.has_value()) {
            emit run_failed(potential_failure2.value());
            return;
        }
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              DownsampleTilesNode* _this = reinterpret_cast<DownsampleTilesNode*>(userdata);
              _this->m_internal_storage_texture.release(); // release texture array when done
              emit _this->run_completed(); // emits signal run_finished()
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_queue, callback_info);
}

std::vector<radix::tile::Id> DownsampleTilesNode::get_tile_ids_for_downsampled_tiles(const std::vector<radix::tile::Id>& original_tile_ids)
{
    std::unordered_set<radix::tile::Id, radix::tile::Id::Hasher> unique_downsampled_tile_ids;
    unique_downsampled_tile_ids.reserve(original_tile_ids.size());
    std::for_each(std::begin(original_tile_ids), std::end(original_tile_ids), [&unique_downsampled_tile_ids](const radix::tile::Id& tile_id) { unique_downsampled_tile_ids.insert(tile_id.parent()); });

    std::vector<radix::tile::Id> downsampled_tile_ids;
    downsampled_tile_ids.reserve(unique_downsampled_tile_ids.size());
    std::for_each(std::begin(unique_downsampled_tile_ids), std::end(unique_downsampled_tile_ids), [&](const radix::tile::Id& tile_id) { downsampled_tile_ids.emplace_back(tile_id); });
    return downsampled_tile_ids;
}

std::optional<NodeRunFailureInfo> DownsampleTilesNode::compute_downsampled_tiles(const std::vector<radix::tile::Id>& tile_ids)
{
    qDebug() << "need to calculate " << tile_ids.size() << " downsampled tiles";

    if (tile_ids.size() > m_input_tile_ids.size()) {
        return NodeRunFailureInfo(*this,
            std::format("failed to store tile ids for downsampling in buffer: trying to store {} tile ids, but buffer size is {}", tile_ids.size(),
                m_input_tile_ids.size()));
    }

    auto& hash_map = *std::get<data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()>(input_socket("hash map").get_connected_data());
    auto& hashmap_textures = *std::get<data_type<TileStorageTexture*>()>(input_socket("textures").get_connected_data());

    if (hashmap_textures.num_used() + tile_ids.size() > hashmap_textures.capacity()) {
        return NodeRunFailureInfo(*this,
            std::format("failed to store textures for downsampling in buffer: texture array has {} layers, where {} layers are already used, tried to store {} "
                        "additional downsampled textures",
                hashmap_textures.capacity(), hashmap_textures.num_used(), tile_ids.size(), m_input_tile_ids.size()));
    }

    std::vector<GpuTileId> gpu_tile_ids;
    gpu_tile_ids.reserve(tile_ids.size());
    std::for_each(std::begin(tile_ids), std::end(tile_ids), [&gpu_tile_ids](const radix::tile::Id& tile_id) { gpu_tile_ids.emplace_back(tile_id.coords.x, tile_id.coords.y, tile_id.zoom_level); });

    m_input_tile_ids.write(m_queue, gpu_tile_ids.data(), gpu_tile_ids.size());

    // bind GPU resources and run pipeline
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "compute: downsample command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "compute: downsample pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(gpu_tile_ids.size(), hashmap_textures.width(), hashmap_textures.height()) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, m_compute_bind_group->handle(), 0, nullptr);
            m_pipeline_manager->downsample_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        // determine which texture array indices to write to for each tile id and copy textures from internal texture to hashmap texture
        for (uint16_t i = 0; i < gpu_tile_ids.size(); i++) {
            size_t layer_index = hashmap_textures.reserve();
            hash_map.store(tile_ids[i], uint32_t(layer_index));
            m_internal_storage_texture->texture().texture().copy_to_texture(encoder.handle(), i, hashmap_textures.texture().texture(), uint32_t(layer_index));
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "compute: downsampling command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    // write texture array indices only after downsampling so we dont accidentally access not-yet-written tiles
    hash_map.update_gpu_data();
    return {};
}

} // namespace webgpu_engine::compute::nodes
