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

#include "UniformBufferObjects.h"
#include "atmosphere/AtmosphereRenderer.h"
#include "cloud/CloudRenderer.h"
#include "nucleus/EngineContext.h"
#include "nucleus/track/Manager.h"
#include "overlay/OverlayRenderer.h"
#include "tile_mesh/TileMeshRenderer.h"
#include "track/TrackRenderer.h"
#include <webgpu/base/Context.h>

namespace webgpu_engine {

class Context : public nucleus::EngineContext {
    Q_OBJECT
public:
    Context(QObject* parent = nullptr);
    Context(Context const&) = delete;
    ~Context() override;
    void operator=(Context const&) = delete;

    TileMeshRenderer* tile_mesh_renderer() const;
    void set_tile_mesh_renderer(std::shared_ptr<TileMeshRenderer> new_tile_mesh_renderer);

    CloudRenderer* cloud_renderer() const;
    void set_cloud_renderer(std::shared_ptr<CloudRenderer> new_cloud_renderer);

    AtmosphereRenderer* atmosphere_renderer() const;
    void set_atmosphere_renderer(std::shared_ptr<AtmosphereRenderer> new_atmosphere_renderer);

    OverlayRenderer* overlay_renderer() const;
    void set_overlay_renderer(std::shared_ptr<OverlayRenderer> new_overlay_renderer);

    TrackRenderer* track_renderer() const;
    void set_track_renderer(std::shared_ptr<TrackRenderer> new_track_renderer);

    webgpu::Context& webgpu_ctx() { return *m_webgpu_ctx_ptr; }
    void set_webgpu_ctx(webgpu::Context& ctx);

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
    webgpu::Context* m_webgpu_ctx_ptr = nullptr;
    uboSharedConfig m_shared_config;
    std::shared_ptr<TileMeshRenderer> m_tile_mesh_renderer;
    std::shared_ptr<CloudRenderer> m_cloud_renderer;
    std::shared_ptr<AtmosphereRenderer> m_atmosphere_renderer;
    std::shared_ptr<OverlayRenderer> m_overlay_renderer;
    std::shared_ptr<TrackRenderer> m_track_renderer;
    // std::shared_ptr<TextureLayer> m_ortho_layer;
};

} // namespace webgpu_engine
