/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "TileGeometry.h"

#include "nucleus/camera/Definition.h"
#include "nucleus/utils/terrain_mesh_index_generator.h"
#include <QDebug>

using webgpu_engine::TileGeometry;

namespace {
template <typename T> int bufferLengthInBytes(const std::vector<T>& vec) { return int(vec.size() * sizeof(T)); }
} // namespace

namespace webgpu_engine {

TileGeometry::TileGeometry(uint32_t height_resolution, uint32_t ortho_resolution)
    : QObject { nullptr }
    , m_height_resolution { height_resolution }
    , m_ortho_resolution { ortho_resolution }
{
}

void TileGeometry::init(WGPUDevice device)
{
    m_device = device;
    m_queue = wgpuDeviceGetQueue(device);

    const auto height_resolution = glm::uvec2(m_height_resolution);
    const auto ortho_resolution = glm::uvec2(m_ortho_resolution);
    const auto num_layers = m_loaded_height_textures.size();

    // create index buffer
    const std::vector<uint16_t> indices = nucleus::utils::terrain_mesh_index_generator::surface_quads_with_curtains<uint16_t>(unsigned(m_height_resolution));
    m_index_buffer = std::make_unique<webgpu::raii::RawBuffer<uint16_t>>(m_device, WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst, indices.size());
    m_index_buffer->write(m_queue, indices.data(), indices.size());
    m_index_buffer_size = indices.size();

    // create buffers for bounds, tile ids, zoom level, height and ortho texture buffers
    m_bounds_buffer = std::make_unique<webgpu::raii::RawBuffer<glm::vec4>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_tileset_id_buffer = std::make_unique<webgpu::raii::RawBuffer<int32_t>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_height_zoom_level_buffer = std::make_unique<webgpu::raii::RawBuffer<int32_t>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_height_texture_layer_buffer = std::make_unique<webgpu::raii::RawBuffer<int32_t>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_ortho_zoom_level_buffer = std::make_unique<webgpu::raii::RawBuffer<int32_t>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_ortho_texture_layer_buffer = std::make_unique<webgpu::raii::RawBuffer<int32_t>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);

    m_tile_id_buffer = std::make_unique<webgpu::raii::RawBuffer<compute::GpuTileId>>(m_device, WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, num_layers);
    m_n_edge_vertices_buffer = std::make_unique<Buffer<int32_t>>(m_device, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    m_n_edge_vertices_buffer->data = int(m_height_resolution);
    m_n_edge_vertices_buffer->update_gpu_data(m_queue);

    WGPUTextureDescriptor height_texture_desc {};
    height_texture_desc.label = WGPUStringView { .data = "height texture", .length = WGPU_STRLEN };
    height_texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    height_texture_desc.size = { uint32_t(height_resolution.x), uint32_t(height_resolution.y), uint32_t(num_layers) };
    height_texture_desc.mipLevelCount = 1;
    height_texture_desc.sampleCount = 1;
    height_texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_R16Uint;
    height_texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUSamplerDescriptor height_sampler_desc {};
    height_sampler_desc.label = WGPUStringView { .data = "height sampler", .length = WGPU_STRLEN };
    height_sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    height_sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    height_sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    // height_sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    // height_sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    // height_sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    height_sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    height_sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    height_sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    height_sampler_desc.lodMinClamp = 0.0f;
    height_sampler_desc.lodMaxClamp = 1.0f;
    height_sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    height_sampler_desc.maxAnisotropy = 1;

    m_heightmap_textures = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, height_texture_desc, height_sampler_desc);

    // TODO mipmaps and compression
    WGPUTextureDescriptor ortho_texture_desc {};
    ortho_texture_desc.label = WGPUStringView { .data = "ortho texture", .length = WGPU_STRLEN };
    ortho_texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    // TODO: array layers might become larger than allowed by graphics API
    ortho_texture_desc.size = { uint32_t(ortho_resolution.x), uint32_t(ortho_resolution.y), uint32_t(num_layers) };
    ortho_texture_desc.mipLevelCount = 1;
    ortho_texture_desc.sampleCount = 1;
    ortho_texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
    ortho_texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUSamplerDescriptor ortho_sampler_desc {};
    ortho_sampler_desc.label = WGPUStringView { .data = "ortho sampler", .length = WGPU_STRLEN };
    ortho_sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    ortho_sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    ortho_sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    ortho_sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    ortho_sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    ortho_sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    ortho_sampler_desc.lodMinClamp = 0.0f;
    ortho_sampler_desc.lodMaxClamp = 1.0f;
    ortho_sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    ortho_sampler_desc.maxAnisotropy = 1;

    m_ortho_textures = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, ortho_texture_desc, ortho_sampler_desc);

    m_tile_bind_group = create_bind_group(m_ortho_textures->texture_view(), m_ortho_textures->sampler());

    m_tile_bind_group = std::make_unique<webgpu::raii::BindGroup>(m_device,
        m_pipeline_manager->tile_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> { m_n_edge_vertices_buffer->raw_buffer().create_bind_group_entry(0),
            m_heightmap_textures->texture_view().create_bind_group_entry(1),
            m_heightmap_textures->sampler().create_bind_group_entry(2),
            m_ortho_textures->texture_view().create_bind_group_entry(3),
            m_ortho_textures->sampler().create_bind_group_entry(4) },
        "tile bind group");
}

void TileGeometry::draw(
    WGPURenderPassEncoder render_pass, const nucleus::camera::Definition& camera, const std::vector<nucleus::tile::TileBounds>& draw_tiles) const
{
    std::vector<glm::vec4> bounds;
    bounds.reserve(draw_tiles.size());

    std::vector<int32_t> tileset_id;
    tileset_id.reserve(draw_tiles.size());

    std::vector<int32_t> height_zoom_levels;
    height_zoom_levels.reserve(draw_tiles.size());

    std::vector<int32_t> height_texture_layer;
    height_texture_layer.reserve(draw_tiles.size());

    std::vector<int32_t> ortho_zoom_levels;
    ortho_zoom_levels.reserve(draw_tiles.size());

    std::vector<int32_t> ortho_texture_layers;
    ortho_texture_layers.reserve(draw_tiles.size());

    std::vector<compute::GpuTileId> tile_ids;
    tile_ids.reserve(draw_tiles.size());

    for (const auto& id_bounds : draw_tiles) {
        const auto& tile_id = id_bounds.id;
        const auto& tile_bounds = id_bounds.bounds;
        bounds.emplace_back(tile_bounds.min.x - camera.position().x, tile_bounds.min.y - camera.position().y, tile_bounds.max.x - camera.position().x, tile_bounds.max.y - camera.position().y);
        tileset_id.emplace_back(tile_id.coords[0] + tile_id.coords[1]);

        const auto height_layer_info = m_loaded_height_textures.layer(tile_id);
        height_zoom_levels.emplace_back(int(height_layer_info.id.zoom_level));
        height_texture_layer.emplace_back(int(height_layer_info.index));

        const auto ortho_layer_info = m_loaded_ortho_textures.layer(tile_id);
        ortho_zoom_levels.emplace_back(int(ortho_layer_info.id.zoom_level));
        ortho_texture_layers.emplace_back(int(ortho_layer_info.index));

        tile_ids.emplace_back(compute::GpuTileId(id_bounds.id));
    }

    // write updated vertex buffers
    m_bounds_buffer->write(m_queue, bounds.data(), bounds.size());
    m_tileset_id_buffer->write(m_queue, tileset_id.data(), tileset_id.size());
    m_height_zoom_level_buffer->write(m_queue, height_zoom_levels.data(), height_zoom_levels.size());
    m_height_texture_layer_buffer->write(m_queue, height_texture_layer.data(), height_texture_layer.size());
    m_ortho_zoom_level_buffer->write(m_queue, ortho_zoom_levels.data(), ortho_zoom_levels.size());
    m_ortho_texture_layer_buffer->write(m_queue, ortho_texture_layers.data(), ortho_texture_layers.size());
    m_tile_id_buffer->write(m_queue, tile_ids.data(), tile_ids.size());

    // set bind group for uniforms, textures and samplers
    wgpuRenderPassEncoderSetBindGroup(render_pass, 2, m_tile_bind_group->handle(), 0, nullptr);

    // set index buffer and vertex buffers
    wgpuRenderPassEncoderSetIndexBuffer(render_pass, m_index_buffer->handle(), WGPUIndexFormat_Uint16, 0, m_index_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_bounds_buffer->handle(), 0, m_bounds_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, m_height_texture_layer_buffer->handle(), 0, m_height_texture_layer_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 2, m_ortho_texture_layer_buffer->handle(), 0, m_ortho_texture_layer_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 3, m_tileset_id_buffer->handle(), 0, m_tileset_id_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 4, m_height_zoom_level_buffer->handle(), 0, m_height_zoom_level_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 5, m_tile_id_buffer->handle(), 0, m_tile_id_buffer->size_in_byte());
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 6, m_ortho_zoom_level_buffer->handle(), 0, m_ortho_zoom_level_buffer->size_in_byte());

    // set pipeline and draw call
    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline_manager->render_tiles_pipeline().pipeline().handle());
    wgpuRenderPassEncoderDrawIndexed(render_pass, uint32_t(m_index_buffer_size), uint32_t(draw_tiles.size()), 0, 0, 0);
}

void TileGeometry::set_tile_limit(unsigned int num_tiles)
{
    m_loaded_height_textures.set_tile_limit(num_tiles);
    m_loaded_ortho_textures.set_tile_limit(num_tiles);
}

void TileGeometry::set_pipeline_manager(const PipelineManager& pipeline_manager) { m_pipeline_manager = &pipeline_manager; }

std::unique_ptr<webgpu::raii::BindGroup> TileGeometry::create_bind_group(const webgpu::raii::TextureView& view, const webgpu::raii::Sampler& sampler) const
{
    return std::make_unique<webgpu::raii::BindGroup>(m_device,
        m_pipeline_manager->tile_bind_group_layout(),
        std::initializer_list<WGPUBindGroupEntry> {
            m_n_edge_vertices_buffer->raw_buffer().create_bind_group_entry(0),
            m_heightmap_textures->texture_view().create_bind_group_entry(1),
            m_heightmap_textures->sampler().create_bind_group_entry(2),
            view.create_bind_group_entry(3),
            sampler.create_bind_group_entry(4),
        },
        "tile bind group");
}

void TileGeometry::update_gpu_tiles_height(const std::vector<radix::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuGeometryTile>& new_tiles)
{
    for (const auto& id : deleted_tiles) {
        m_loaded_height_textures.remove_tile(id);
    }

    for (const auto& tile : new_tiles) {
        // test for validity
        assert(tile.id.zoom_level < 100);
        assert(tile.surface);

        // find empty spot and upload texture
        const uint32_t layer_index = m_loaded_height_textures.add_tile(tile.id);
        m_heightmap_textures->texture().write(m_queue, *tile.surface, layer_index);
    }
}

void TileGeometry::update_gpu_tiles_ortho(const std::vector<nucleus::tile::Id>& deleted_tiles, const std::vector<nucleus::tile::GpuTextureTile>& new_tiles)
{
    for (const auto& id : deleted_tiles) {
        m_loaded_ortho_textures.remove_tile(id);
    }
    for (const auto& tile : new_tiles) {
        // test for validity
        assert(tile.id.zoom_level < 100);
        assert(tile.texture);

        // find empty spot and upload texture
        const auto layer_index = m_loaded_ortho_textures.add_tile(tile.id);
        m_ortho_textures->texture().write(m_queue, tile.texture->front(), uint32_t(layer_index));
    }
}

} // namespace webgpu_engine
