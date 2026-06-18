/*****************************************************************************
 * weBIGeo
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

#include "LoadTextureNode.h"
#include "util.h"

#include "nucleus/utils/image_loader.h"

namespace webgpu_compute::nodes {

LoadTextureNode::LoadTextureNode(webgpu::Context& ctx)
    : LoadTextureNode(ctx, LoadTextureNodeSettings())
{
}

LoadTextureNode::LoadTextureNode(webgpu::Context& ctx, const LoadTextureNodeSettings& settings)
    : Node({},
          {
              OutputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_output_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings(settings)
{
}

void LoadTextureNode::set_settings(const LoadTextureNodeSettings& settings) { m_settings = settings; }

void LoadTextureNode::run_impl()
{

    qDebug() << "loading texture from " << m_settings.file_path;

    auto path = QString::fromStdString(m_settings.file_path);
    tl::expected<nucleus::Raster<glm::u8vec4>, QString> expected_image = nucleus::utils::image_loader::rgba8(path);
    if (!expected_image.has_value()) {
        fail_run("Failed to load image file at " + m_settings.file_path + ": " + expected_image.error().toStdString());
        return;
    }

    nucleus::Raster<glm::u8vec4> image = expected_image.value();
    m_output_texture = create_texture(m_ctx->device(), image.width(), image.height(), m_settings.format, m_settings.usage);
    m_output_texture->texture().write(m_ctx->queue(), image);

    // TODO not sure if we need to wait for the queue here?
    complete_run();
}

std::unique_ptr<webgpu::raii::TextureWithSampler> LoadTextureNode::create_texture(
    WGPUDevice device, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    // create output texture
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "LoadTextureNode output texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { width, height, 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;

    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "LoadTextureNode sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    return std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);
}

void LoadTextureNode::serialize_settings(QJsonObject& out) const
{
    out["file_path"] = QString::fromStdString(m_settings.file_path);
    out["format"] = wgpu_format_to_string(m_settings.format);
    out["usage"] = wgpu_usage_to_json(m_settings.usage);
}

void LoadTextureNode::deserialize_settings(const QJsonObject& in)
{
    auto s = m_settings;
    if (in.contains("file_path"))
        s.file_path = in["file_path"].toString().toStdString();
    if (in.contains("format"))
        s.format = wgpu_format_from_string(in["format"].toString(), s.format);
    if (in.contains("usage"))
        s.usage = wgpu_usage_from_json(in["usage"].toArray(), s.usage);
    set_settings(s);
}

} // namespace webgpu_compute::nodes
