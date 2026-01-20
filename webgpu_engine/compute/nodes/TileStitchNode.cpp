/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "TileStitchNode.h"

#include <QDebug>
#include <QString>
#include <nucleus/srs.h>
#include <nucleus/utils/image_loader.h>
#include <nucleus/utils/image_writer.h>
#include <qdir.h>

namespace webgpu_engine::compute::nodes {

webgpu_engine::compute::nodes::TileStitchNode::TileStitchNode(const PipelineManager& manager, WGPUDevice device, StitchSettings settings)
    : Node(
          {
              InputSocket(*this, "tile ids", data_type<const std::vector<radix::tile::Id>*>()),
              InputSocket(*this, "texture data", data_type<const std::vector<QByteArray>*>()),
          },
          { OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }) })
    , m_pipeline_manager(&manager)
    , m_device(device)
    , m_queue(wgpuDeviceGetQueue(m_device))
    , m_settings(settings)
{
}

void TileStitchNode::run_impl()
{
    qDebug() << "running TileStitchNode ...";

    // get tile ids to process
    const auto& tile_ids = *std::get<data_type<const std::vector<radix::tile::Id>*>()>(input_socket("tile ids").get_connected_data());
    const auto& textures = *std::get<data_type<const std::vector<QByteArray>*>()>(input_socket("texture data").get_connected_data());
    assert(tile_ids.size() == textures.size());

    // The original tile size is as configured in the settings
    glm::uvec2 so = m_settings.tile_size;
    // The effective tile size depends on whether they include a 1px border
    glm::uvec2 s = m_settings.tile_has_border ? so - glm::uvec2(1) : so;
    // The zoom level of the stitched depends on the zoom level of the first tile inside the tile_ids (could be a setting)
    uint32_t zl = tile_ids[0].zoom_level;

    // Iterate through all tiles and save the bounds for the zoom level
    glm::uvec4 bounds = glm::uvec4(
        std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::min());
    for (const auto& tile_id : tile_ids) {
        bounds.x = std::min(bounds.x, tile_id.coords.x);
        bounds.y = std::min(bounds.y, tile_id.coords.y);
        bounds.z = std::max(bounds.z, tile_id.coords.x);
        bounds.w = std::max(bounds.w, tile_id.coords.y);
    }

    // Calculate the size in tiles and pixel
    glm::uvec2 size_tiles;
    glm::uvec2 size_pixels;
    size_tiles = glm::uvec2(bounds.z - bounds.x + 1, bounds.w - bounds.y + 1);
    size_pixels = size_tiles * s;

    qDebug() << "About to stitch " << size_tiles.x << "x" << size_tiles.y << " tiles into an image of size " << size_pixels.x << "x" << size_pixels.y
             << " pixels";

    // Check if inside bounds
    if (size_pixels.x > MAX_STITCHED_IMAGE_SIZE || size_pixels.y > MAX_STITCHED_IMAGE_SIZE) {
        emit run_failed(NodeRunFailureInfo(*this,
            std::format(
                "Stitched image size would exceeds maximum size of {}x{} pixel for zoom level {}", MAX_STITCHED_IMAGE_SIZE, MAX_STITCHED_IMAGE_SIZE, zl)));
        return;
    }

    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "compute storage texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { size_pixels.x, size_pixels.y, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = m_settings.texture_format;
    texture_desc.usage = m_settings.texture_usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "compute storage sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    m_output_texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);
    auto& tex = m_output_texture->texture();

    // store them in this context, otherwise they get deleted too soon (might not be necessary...)
    std::map<size_t, nucleus::Raster<glm::u8vec4>> images;
    // Load the tiles and upload them directly to the gpu texture
    for (size_t i = 0; i < tile_ids.size(); i++) {
        const auto& tile_id = tile_ids[i];
        const auto& texture_data = textures[i];

        if (tile_id.zoom_level != zl)
            continue;

        // Calculate the position of the tile in the stitched image
        glm::uvec2 pos = glm::uvec2(tile_id.coords.x - bounds.x, tile_id.coords.y - bounds.y) * s;
        if (m_settings.stitch_inverted_y) {
            pos.y = size_pixels.y - pos.y - s.y;
        }

        // Load image (NOTE: Only supports u8vec4 so far)
        images[i] = nucleus::utils::image_loader::rgba8(texture_data).value();
        const auto& image = images[i];
        assert(image.width() == so.x && image.height() == so.y);

        WGPUTexelCopyTextureInfo image_copy_texture {};
        image_copy_texture.texture = tex.handle();
        image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
        image_copy_texture.mipLevel = 0;
        image_copy_texture.origin = { pos.x, pos.y, 0 };

        WGPUTexelCopyBufferLayout texture_data_layout {};
        texture_data_layout.bytesPerRow = uint32_t(sizeof(glm::u8vec4) * so.x);
        texture_data_layout.rowsPerImage = uint32_t(so.y);
        texture_data_layout.offset = 0;

        WGPUExtent3D copy_extent {};
        copy_extent.width = s.x;
        copy_extent.height = s.y;
        copy_extent.depthOrArrayLayers = 1;

        wgpuQueueWriteTexture(m_queue, &image_copy_texture, image.bytes(), uint32_t(image.size_in_bytes()), &texture_data_layout, &copy_extent);
    }

    emit this->run_completed(); // emits signal run_finished()

    // Weird that this works here. Is wgpuQueueWriteTexture blocking after all?
    // m_output_texture->texture().save_to_file(m_device, "C:\\tmp\\asd.png");
}

} // namespace webgpu_engine::compute::nodes
