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

#include "compute.h"
#include "nucleus/utils/tile_conversion.h"

namespace webgpu_engine {

std::vector<tile::Id> RectangularTileRegion::get_tiles() const
{
    assert(min.x <= max.x);
    assert(min.y <= max.y);
    std::vector<tile::Id> tiles;
    tiles.reserve((min.x - max.x + 1) * (min.y - max.y + 1));
    for (unsigned x = min.x; x <= max.x; x++) {
        for (unsigned y = min.y; y <= max.y; y++) {
            tiles.emplace_back(tile::Id { zoom_level, { x, y }, scheme });
        }
    }
    return tiles;
}

TextureArrayComputeTileStorage::TextureArrayComputeTileStorage(
    WGPUDevice device, const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format, WGPUTextureUsageFlags usage)
    : m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_resolution { resolution }
    , m_capacity { capacity }
{
    WGPUTextureDescriptor height_texture_desc {};
    height_texture_desc.label = "compute storage texture";
    height_texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    height_texture_desc.size = { uint32_t(m_resolution.x), uint32_t(m_resolution.y), uint32_t(m_capacity) };
    height_texture_desc.mipLevelCount = 1;
    height_texture_desc.sampleCount = 1;
    height_texture_desc.format = format;
    height_texture_desc.usage = usage;

    WGPUSamplerDescriptor height_sampler_desc {};
    height_sampler_desc.label = "compute storage sampler";
    height_sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    height_sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    height_sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    height_sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    height_sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    height_sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    height_sampler_desc.lodMinClamp = 0.0f;
    height_sampler_desc.lodMaxClamp = 1.0f;
    height_sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    height_sampler_desc.maxAnisotropy = 1;

    m_texture_array = std::make_unique<raii::TextureWithSampler>(m_device, height_texture_desc, height_sampler_desc);

    m_tile_ids = std::make_unique<raii::RawBuffer<GpuTileId>>(
        m_device, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, uint32_t(m_capacity), "compute tile storage tile id buffer");

    m_layer_index_to_tile_id.clear();
    m_layer_index_to_tile_id.resize(m_capacity, tile::Id { unsigned(-1), {} });
}

void TextureArrayComputeTileStorage::init()
{

}

void TextureArrayComputeTileStorage::store(const tile::Id& id, std::shared_ptr<QByteArray> data)
{
    // TODO maybe rather use hash map than list

    // already contained, return
    if (std::find(m_layer_index_to_tile_id.begin(), m_layer_index_to_tile_id.end(), id) != m_layer_index_to_tile_id.end()) {
        return;
    }

    // find free spot
    const auto found = std::find(m_layer_index_to_tile_id.begin(), m_layer_index_to_tile_id.end(), tile::Id { unsigned(-1), {} });
    if (found == m_layer_index_to_tile_id.end()) {
        // TODO capacity is reached! do something (but what?)
    }
    *found = id;
    const size_t found_index = found - m_layer_index_to_tile_id.begin();

    // convert to raster and store in texture array
    const auto raster = nucleus::utils::tile_conversion::qImage2uint16Raster(nucleus::utils::tile_conversion::toQImage(*data));
    m_texture_array->texture().write(m_queue, raster, uint32_t(found_index));

    GpuTileId gpu_tile_id = { .x = id.coords.x, .y = id.coords.y, .zoomlevel = id.zoom_level };
    m_tile_ids->write(m_queue, &gpu_tile_id, 1, found_index);
}

void TextureArrayComputeTileStorage::clear(const tile::Id& id)
{
    auto found = std::find(m_layer_index_to_tile_id.begin(), m_layer_index_to_tile_id.end(), id);
    if (found != m_layer_index_to_tile_id.end()) {
        *found = tile::Id { unsigned(-1), {} };
    }
}

void TextureArrayComputeTileStorage::read_back_async(size_t layer_index, ReadBackCallback callback)
{
    size_t buffer_size_bytes = 4 * m_resolution.x * m_resolution.y; // RGBA8 -> hardcoded, depends actual on format used!

    // create buffer and add buffer and callback to back of queue
    m_read_back_states.emplace(std::make_unique<raii::RawBuffer<char>>(
                                   m_device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, uint32_t(buffer_size_bytes), "tile storage read back buffer"),
        callback, layer_index);

    m_texture_array->texture().copy_to_buffer(m_device, *m_read_back_states.back().buffer, uint32_t(layer_index));

    auto on_buffer_mapped = [](WGPUBufferMapAsyncStatus status, void* user_data) {
        TextureArrayComputeTileStorage* _this = reinterpret_cast<TextureArrayComputeTileStorage*>(user_data);

        if (status != WGPUBufferMapAsyncStatus_Success) {
            std::cout << "error: failed mapping buffer for ComputeTileStorage read back" << std::endl;
            _this->m_read_back_states.pop();
            return;
        }

        const ReadBackState& current_state = _this->m_read_back_states.front();
        size_t buffer_size_bytes = 4 * _this->m_resolution.x * _this->m_resolution.y; // RGBA8 -> hardcoded, depends actual on format used!
        const char* buffer_data = (const char*)wgpuBufferGetConstMappedRange(current_state.buffer->handle(), 0, buffer_size_bytes);
        current_state.callback(current_state.layer_index, std::make_shared<QByteArray>(buffer_data, buffer_size_bytes));
        wgpuBufferUnmap(current_state.buffer->handle());

        _this->m_read_back_states.pop();
    };

    wgpuBufferMapAsync(m_read_back_states.back().buffer->handle(), WGPUMapMode_Read, 0, uint32_t(buffer_size_bytes), on_buffer_mapped, this);
}

std::vector<WGPUBindGroupEntry> TextureArrayComputeTileStorage::create_bind_group_entries(const std::vector<uint32_t>& bindings) const
{
    assert(bindings.size() == 1 || bindings.size() == 2);
    if (bindings.size() == 1) {
        return { m_texture_array->texture_view().create_bind_group_entry(bindings.at(0)) };
    }
    return { m_texture_array->texture_view().create_bind_group_entry(bindings.at(0)), m_tile_ids->create_bind_group_entry(bindings.at(1)) };
}

ComputeController::ComputeController(WGPUDevice device, const PipelineManager& pipeline_manager)
    : m_pipeline_manager(&pipeline_manager)
    , m_device { device }
    , m_queue { wgpuDeviceGetQueue(m_device) }
    , m_tile_loader { std::make_unique<nucleus::tile_scheduler::TileLoadService>(
          "https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY, ".png") }
    , m_input_tile_storage { std::make_unique<TextureArrayComputeTileStorage>(
          device, m_input_tile_resolution, m_max_num_tiles, WGPUTextureFormat_R16Uint, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst) }
    , m_output_tile_storage { std::make_unique<TextureArrayComputeTileStorage>(device, m_output_tile_resolution, m_max_num_tiles, WGPUTextureFormat_RGBA8Unorm,
          WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc) }
{
    connect(m_tile_loader.get(), &nucleus::tile_scheduler::TileLoadService::load_finished, this, &ComputeController::on_single_tile_received);

    m_input_tile_storage->init();
    m_output_tile_storage->init();

    std::vector<WGPUBindGroupEntry> entries;
    std::vector<WGPUBindGroupEntry> input_entries = m_input_tile_storage->create_bind_group_entries({ 0, 1 });
    std::vector<WGPUBindGroupEntry> output_entries = m_output_tile_storage->create_bind_group_entries({ 2 });
    entries.insert(entries.end(), input_entries.begin(), input_entries.end());
    entries.insert(entries.end(), output_entries.begin(), output_entries.end());
    m_compute_bind_group = std::make_unique<raii::BindGroup>(device, pipeline_manager.compute_bind_group_layout(), entries, "compute controller bind group");
}

void ComputeController::request_tiles(const RectangularTileRegion& region)
{
    std::vector<tile::Id> tiles_in_region = region.get_tiles();
    assert(tiles_in_region.size() <= m_max_num_tiles);
    m_num_tiles_requested = tiles_in_region.size();
    std::cout << "requested " << m_num_tiles_requested << " tiles" << std::endl;
    m_num_tiles_received = 0;
    for (const auto& tile : tiles_in_region) {
        m_tile_loader->load(tile);
    }
}

void ComputeController::run_pipeline()
{
    WGPUCommandEncoderDescriptor descriptor {};
    descriptor.label = "compute controller command encoder";
    raii::CommandEncoder encoder(m_device, descriptor);

    {
        WGPUComputePassDescriptor compute_pass_desc {};
        compute_pass_desc.label = "compute controller compute pass";
        raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

        const glm::uvec3& workgroup_counts = { m_max_num_tiles, 1, 1 };
        wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, m_compute_bind_group->handle(), 0, nullptr);
        m_pipeline_manager->dummy_compute_pipeline().run(compute_pass, workgroup_counts);
    }

    WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
    cmd_buffer_descriptor.label = "computr controller command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuQueueOnSubmittedWorkDone(
        m_queue,
        []([[maybe_unused]] WGPUQueueWorkDoneStatus status, void* user_data) {
            ComputeController* _this = reinterpret_cast<ComputeController*>(user_data);
            std::cout << "pipeline run done" << std::endl;
            _this->pipeline_done(); // emits signal pipeline_done()
        },
        this);
}

void ComputeController::write_output_tiles(const std::filesystem::path& dir) const
{
    std::filesystem::create_directories(dir);

    auto read_back_callback = [this, dir](size_t layer_index, std::shared_ptr<QByteArray> data) {
        QImage img((const uchar*)data->constData(), m_output_tile_resolution.x, m_output_tile_resolution.y, QImage::Format_RGBA8888);
        std::filesystem::path file_path = dir / std::format("tile_{}.png", layer_index);
        std::cout << "write to file " << file_path << std::endl;
        img.save(file_path.generic_string().c_str(), "PNG");
    };

    std::cout << "write to files" << std::endl;
    for (size_t i = 0; i < m_max_num_tiles; i++) {
        m_output_tile_storage->read_back_async(i, read_back_callback);
    }
}

void ComputeController::on_single_tile_received(const nucleus::tile_scheduler::tile_types::TileLayer& tile)
{
    std::cout << "received requested tile " << tile.id << std::endl;
    m_input_tile_storage->store(tile.id, tile.data);
    m_num_tiles_received++;
    if (m_num_tiles_received == m_num_tiles_requested) {
        on_all_tiles_received();
    }
}

void ComputeController::on_all_tiles_received()
{
    std::cout << "received " << m_num_tiles_requested << " tiles, running pipeline" << std::endl;
    run_pipeline();
}

} // namespace webgpu_engine
