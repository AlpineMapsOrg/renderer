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

#include "CreateHashMapNode.h"

#include <QDebug>

namespace webgpu_engine::compute::nodes {

CreateHashMapNode::CreateHashMapNode(WGPUDevice device, const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format)
    : Node(
          {
              InputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>()),
              InputSocket(*this, "texture data", data_type<const std::vector<QByteArray>*>()),
          },
          {
              OutputSocket(*this, "hash map", data_type<GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*>(), [this]() { return &m_output_tile_id_to_index; }),
              OutputSocket(*this, "textures", data_type<TileStorageTexture*>(), [this]() { return &m_output_tile_textures; }),
          })
    , m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_output_tile_id_to_index(device, radix::tile::Id { UINT32_MAX, {} }, UINT32_MAX)
    , m_output_tile_textures(device, resolution, capacity, format, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc)
{
    m_output_tile_id_to_index.update_gpu_data();
}

void CreateHashMapNode::run_impl()
{
    qDebug() << "running ConvertToHashMapNode ...";

    // get input data
    // TODO maybe make get_input_data a template (so usage would become get_input_data<type>(socket_index))
    const auto& tile_ids = *std::get<data_type<const std::vector<radix::tile::Id>*>()>(input_socket("tile ids").get_connected_data()); // input 1, list of tile ids
    const auto& textures = *std::get<data_type<const std::vector<QByteArray>*>()>(
        input_socket("texture data").get_connected_data()); // input 2, list of tile corresponding textures

    assert(tile_ids.size() == textures.size());

    if (tile_ids.size() > m_output_tile_textures.capacity()) {
        emit run_failed(NodeRunFailureInfo(*this,
            std::format("failed to store textures in GPU hash map: trying to store {} textures, but texture array capacity is {}", tile_ids.size(),
                m_output_tile_textures.capacity())));
        return;
    }

    qDebug() << "populating hash map with " << tile_ids.size() << " entries";

    // store each texture in texture array and store resulting index in hashmap
    m_output_tile_id_to_index.clear();
    m_output_tile_textures.clear();
    for (size_t i = 0; i < tile_ids.size(); i++) {
        auto index = m_output_tile_textures.store(textures[i]);
        m_output_tile_id_to_index.store(tile_ids[i], uint32_t(index));
    }
    m_output_tile_id_to_index.update_gpu_data();

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              CreateHashMapNode* _this = reinterpret_cast<CreateHashMapNode*>(userdata);
              // qDebug() << "hash=" << gpu_hash(tile::Id { 11, { 1096, 1328 } }) << Qt::endl;
              // std::cout << " done, reading back buffer for debugging purposes..." << std::endl;
              // std::vector<GpuTileId> key_buffer_contents = _this->m_output_tile_id_to_index.key_buffer().read_back_sync(_this->m_device, 10000);
              // std::vector<uint32_t> value_buffer_contents = _this->m_output_tile_id_to_index.value_buffer().read_back_sync(_this->m_device, 10000);
              // std::cout << "done" << std::endl;

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
