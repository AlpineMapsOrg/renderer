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

#pragma once

#include "CloudRenderer.h"
#include "TileGeometry.h"
#include "UniformBufferObjects.h"
#include "nucleus/EngineContext.h"
#include "nucleus/track/Manager.h"

namespace webgpu_engine {

class Context : public nucleus::EngineContext {
    Q_OBJECT
public:
    Context(QObject* parent = nullptr);
    Context(Context const&) = delete;
    ~Context() override;
    void operator=(Context const&) = delete;

    TileGeometry* tile_geometry() const;
    void set_tile_geometry(std::shared_ptr<TileGeometry> new_tile_geometry);

    CloudRenderer* cloud_geometry() const;
    void set_cloud_geometry(std::shared_ptr<CloudRenderer> new_cloud_geometry);

    WGPUInstance webgpu_instance() const;
    void set_webgpu_instance(WGPUInstance instance);

    WGPUDevice webgpu_device() const;
    void set_webgpu_device(WGPUDevice device);

    webgpu_engine::ShaderModuleManager* shader_module_manager();

    webgpu_engine::PipelineManager* pipeline_manager();

    nucleus::track::Manager* track_manager() override;

    uboSharedConfig& shared_config();
    void request_redraw();

    // TODO: add after getting merge to work
    // TextureLayer* ortho_layer() const;
    // void set_ortho_layer(std::shared_ptr<TextureLayer> new_ortho_layer);

signals:
    void redraw_requested();

protected:
    void internal_initialise() override;
    void internal_destroy() override;

private:
    WGPUDevice m_webgpu_device = 0;
    WGPUInstance m_webgpu_instance = 0;
    uboSharedConfig m_shared_config;
    std::shared_ptr<TileGeometry> m_tile_geometry;
    std::shared_ptr<CloudRenderer> m_cloud_geometry;
    // std::shared_ptr<TextureLayer> m_ortho_layer;

    std::unique_ptr<webgpu_engine::ShaderModuleManager> m_shader_module_manager;
    std::unique_ptr<webgpu_engine::PipelineManager> m_pipeline_manager;
};

} // namespace webgpu_engine
