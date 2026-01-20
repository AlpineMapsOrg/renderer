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

#include "ComputeD8DirectionsNode.h"

namespace webgpu_engine::compute::nodes {

glm::uvec3 ComputeD8DirectionsNode::SHADER_WORKGROUP_SIZE = { 1, 16, 16 };

ComputeD8DirectionsNode::ComputeD8DirectionsNode(
    const PipelineManager& pipeline_manager, WGPUDevice device, const glm::uvec2& output_resolution, size_t capacity)
    : Node(
          {
              InputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>()),
              InputSocket(*this, "hash map", data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()),
              InputSocket(*this, "height textures", data_type<TileStorageTexture*>()),
          },
          {
              OutputSocket(*this, "hash map", data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>(), [this]() { return &m_output_tile_map; }),
              OutputSocket(*this, "d8 direction textures", data_type<TileStorageTexture*>(), [this]() { return &m_output_texture; }),
          })
    , m_pipeline_manager(&pipeline_manager)
    , m_device(device)
    , m_queue(wgpuDeviceGetQueue(device))
    , m_capacity(capacity)
    , m_input_tile_ids(device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, capacity, "d8 direction compute, tile id buffer")
    , m_output_tile_map(device, radix::tile::Id { unsigned(-1), {} }, UINT32_MAX)
    , m_output_texture(device,
          output_resolution,
          capacity,
          WGPUTextureFormat_RGBA8Unorm,
          WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc)
{
    m_output_tile_map.clear();
    m_output_tile_map.update_gpu_data();
}

void ComputeD8DirectionsNode::run_impl()
{
    qDebug() << "running ComputeD8DirectionsNode ...";

    // get input data
    const auto& tile_ids = *std::get<data_type<const std::vector<radix::tile::Id>*>()>(input_socket("tile ids").get_connected_data());
    const auto& hash_map = *std::get<data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>()>(input_socket("hash map").get_connected_data());
    const auto& height_textures = *std::get<data_type<TileStorageTexture*>()>(input_socket("height textures").get_connected_data());

    if (tile_ids.size() > m_output_texture.capacity()) {
        emit run_failed(NodeRunFailureInfo(*this,
            std::format("failed to store textures in GPU hash map: trying to store {} textures, but hash map capacity is {}", tile_ids.size(),
                m_output_texture.capacity())));
        return;
    }

    // calculate bounds per tile id, write tile ids and bounds to buffer
    std::vector<GpuTileId> gpu_tile_ids(tile_ids.size());
    for (size_t i = 0; i < gpu_tile_ids.size(); i++) {
        gpu_tile_ids[i] = { tile_ids[i].coords.x, tile_ids[i].coords.y, tile_ids[i].zoom_level };
    }
    m_input_tile_ids.write(m_queue, gpu_tile_ids.data(), gpu_tile_ids.size());

    // create bind group
    // TODO re-create bind groups only when input handles change
    // TODO adapter shader code
    // TODO compute bounds in other node!
    std::vector<WGPUBindGroupEntry> entries {
        m_input_tile_ids.create_bind_group_entry(0),
        hash_map.key_buffer().create_bind_group_entry(1),
        hash_map.value_buffer().create_bind_group_entry(2),
        height_textures.texture().texture_view().create_bind_group_entry(3),
        height_textures.texture().sampler().create_bind_group_entry(4),
        m_output_texture.texture().texture_view().create_bind_group_entry(5),
    };
    webgpu::raii::BindGroup compute_bind_group(m_device, m_pipeline_manager->d8_compute_bind_group_layout(), entries, "compute d8 bind group");

    // bind GPU resources and run pipeline
    // the result is a texture array with the calculated overlays, and a hashmap that maps id to texture array index
    // the shader will only writes into texture array, the hashmap is written on cpu side
    {
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "compute d8 command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_device, descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "compute d8 compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(tile_ids.size(), m_output_texture.width(), m_output_texture.height()) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline_manager->d8_compute_pipeline().run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "compute d8 command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    // write hashmap
    // since the compute pass stores textures at indices [0, num_tile_ids), we can just write those indices into the hashmap
    m_output_tile_map.clear();
    m_output_texture.clear();
    for (uint16_t i = 0; i < tile_ids.size(); i++) {
        m_output_texture.reserve(i);
        m_output_tile_map.store(tile_ids[i], i);
    }
    m_output_tile_map.update_gpu_data();

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeD8DirectionsNode* _this = reinterpret_cast<ComputeD8DirectionsNode*>(userdata);
              emit _this->run_completed();
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

} // namespace webgpu_engine::compute::nodes
