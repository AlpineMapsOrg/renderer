/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "Context.h"

#include <webgpu/base/raii/BindGroupLayout.h>

namespace webgpu_engine {

Context::Context(QObject* parent)
    : nucleus::EngineContext(parent)
{
}

Context::~Context() = default;

void Context::internal_initialise()
{
    assert(m_webgpu_ctx_ptr != nullptr);

    auto& reg = webgpu_ctx().resource_registry();
    reg.set_local_shader_path("webgpu", ALP_SHADER_DIR_WEBGPU);
    reg.set_local_shader_path("webgpu_engine", ALP_SHADER_DIR_WEBGPU_ENGINE);

    reg.register_bind_group_layout("shared_config", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = 0;
        entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.minBindingSize = 0;
        return std::make_unique<webgpu::raii::BindGroupLayout>(device, std::vector<WGPUBindGroupLayoutEntry> { entry }, "shared config bind group layout");
    });

    reg.register_bind_group_layout("camera", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = 0;
        entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.minBindingSize = 0;
        return std::make_unique<webgpu::raii::BindGroupLayout>(device, std::vector<WGPUBindGroupLayoutEntry> { entry }, "camera bind group layout");
    });

    reg.register_bind_group_layout("depth_texture", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry entry {};
        entry.binding = 0;
        entry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
        entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        return std::make_unique<webgpu::raii::BindGroupLayout>(device, std::vector<WGPUBindGroupLayoutEntry> { entry }, "depth texture bind group layout");
    });

    reg.register_bind_group_layout("compose", [](WGPUDevice device) {
        WGPUBindGroupLayoutEntry albedo_entry {};
        albedo_entry.binding = 0;
        albedo_entry.visibility = WGPUShaderStage_Fragment;
        albedo_entry.texture.sampleType = WGPUTextureSampleType_Uint;
        albedo_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry position_entry {};
        position_entry.binding = 1;
        position_entry.visibility = WGPUShaderStage_Fragment;
        position_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        position_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry normal_entry {};
        normal_entry.binding = 2;
        normal_entry.visibility = WGPUShaderStage_Fragment;
        normal_entry.texture.sampleType = WGPUTextureSampleType_Uint;
        normal_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry atmosphere_entry {};
        atmosphere_entry.binding = 3;
        atmosphere_entry.visibility = WGPUShaderStage_Fragment;
        atmosphere_entry.texture.sampleType = WGPUTextureSampleType_Float;
        atmosphere_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry overlay_entry {};
        overlay_entry.binding = 4;
        overlay_entry.visibility = WGPUShaderStage_Fragment;
        overlay_entry.texture.sampleType = WGPUTextureSampleType_Uint;
        overlay_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry clouds_texture_entry {};
        clouds_texture_entry.binding = 5;
        clouds_texture_entry.visibility = WGPUShaderStage_Fragment;
        clouds_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
        clouds_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry clouds_depth_entry {};
        clouds_depth_entry.binding = 6;
        clouds_depth_entry.visibility = WGPUShaderStage_Fragment;
        clouds_depth_entry.storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
        clouds_depth_entry.storageTexture.format = WGPUTextureFormat_R32Float;
        clouds_depth_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry shadow_texture_entry {};
        shadow_texture_entry.binding = 7;
        shadow_texture_entry.visibility = WGPUShaderStage_Fragment;
        shadow_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
        shadow_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry shadow_sampler_entry {};
        shadow_sampler_entry.binding = 8;
        shadow_sampler_entry.visibility = WGPUShaderStage_Fragment;
        shadow_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutEntry depth_texture_entry {};
        depth_texture_entry.binding = 9;
        depth_texture_entry.visibility = WGPUShaderStage_Fragment;
        depth_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        depth_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry overlay_renderer_post_entry {};
        overlay_renderer_post_entry.binding = 10;
        overlay_renderer_post_entry.visibility = WGPUShaderStage_Fragment;
        overlay_renderer_post_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        overlay_renderer_post_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry overlay_renderer_pre_entry {};
        overlay_renderer_pre_entry.binding = 11;
        overlay_renderer_pre_entry.visibility = WGPUShaderStage_Fragment;
        overlay_renderer_pre_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        overlay_renderer_pre_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

        return std::make_unique<webgpu::raii::BindGroupLayout>(device,
            std::vector<WGPUBindGroupLayoutEntry> {
                albedo_entry,
                position_entry,
                normal_entry,
                atmosphere_entry,
                overlay_entry,
                clouds_texture_entry,
                clouds_depth_entry,
                shadow_texture_entry,
                shadow_sampler_entry,
                depth_texture_entry,
                overlay_renderer_post_entry,
                overlay_renderer_pre_entry,
            },
            "compose bind group layout");
    });

    if (m_tile_mesh_renderer)
        m_tile_mesh_renderer->init(webgpu_ctx());
    if (m_atmosphere_renderer)
        m_atmosphere_renderer->init(webgpu_ctx());
    if (m_cloud_renderer)
        m_cloud_renderer->init(webgpu_ctx());
    if (m_overlay_renderer)
        m_overlay_renderer->init(*this);
    if (m_track_renderer)
        m_track_renderer->init(webgpu_ctx());
    // if (m_ortho_layer)
    //     m_ortho_layer->init();
}

void Context::internal_destroy()
{
    // this is necessary for a clean shutdown (and we want a clean shutdown for the ci integration test).
    // m_ortho_layer.reset();
    m_track_renderer.reset();
    m_overlay_renderer.reset();
    m_cloud_renderer.reset();
    m_atmosphere_renderer.reset();
    m_tile_mesh_renderer.reset();
}

TileMeshRenderer* Context::tile_mesh_renderer() const { return m_tile_mesh_renderer.get(); }

void Context::set_tile_mesh_renderer(std::shared_ptr<TileMeshRenderer> new_tile_mesh_renderer)
{
    assert(!is_alive()); // only set before init is called.
    m_tile_mesh_renderer = std::move(new_tile_mesh_renderer);
}

CloudRenderer* Context::cloud_renderer() const { return m_cloud_renderer.get(); }

void Context::set_cloud_renderer(std::shared_ptr<CloudRenderer> new_cloud_renderer)
{
    assert(!is_alive()); // only set before init is called.
    m_cloud_renderer = std::move(new_cloud_renderer);
}

AtmosphereRenderer* Context::atmosphere_renderer() const { return m_atmosphere_renderer.get(); }

void Context::set_atmosphere_renderer(std::shared_ptr<AtmosphereRenderer> new_atmosphere_renderer)
{
    assert(!is_alive()); // only set before init is called.
    m_atmosphere_renderer = std::move(new_atmosphere_renderer);
}

OverlayRenderer* Context::overlay_renderer() const { return m_overlay_renderer.get(); }

void Context::set_overlay_renderer(std::shared_ptr<OverlayRenderer> new_overlay_renderer)
{
    assert(!is_alive()); // only set before init is called.
    m_overlay_renderer = std::move(new_overlay_renderer);
}

TrackRenderer* Context::track_renderer() const { return m_track_renderer.get(); }

void Context::set_track_renderer(std::shared_ptr<TrackRenderer> new_track_renderer)
{
    assert(!is_alive()); // only set before init is called.
    m_track_renderer = std::move(new_track_renderer);
}

void Context::set_webgpu_ctx(webgpu::Context& ctx) { m_webgpu_ctx_ptr = &ctx; }

nucleus::track::Manager* Context::track_manager() { return nullptr; }

uboSharedConfig& Context::shared_config() { return m_shared_config; }

void Context::request_redraw() { emit redraw_requested(); }

/*TextureLayer* Context::ortho_layer() const { return m_ortho_layer.get(); }

void Context::set_ortho_layer(std::shared_ptr<TextureLayer> new_ortho_layer)
{
    assert(!is_alive()); // only set before init is called.
    m_ortho_layer = std::move(new_ortho_layer);
}*/

} // namespace webgpu_engine
