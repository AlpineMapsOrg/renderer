/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include <memory>

#include <QObject>
#include <webgpu/base/Context.h>
#include <webgpu/base/Framebuffer.h>
#include <webgpu/base/raii/Pipeline.h>
#include <webgpu/base/raii/TextureView.h>
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class AtmosphereRenderer : public QObject {
    Q_OBJECT
public:
    explicit AtmosphereRenderer();

    void init(webgpu::Context& ctx);

    void resize(int w, int h);

    void draw(const WGPUCommandEncoder& command_encoder, const WGPUBindGroup& camera_bind_group);

    [[nodiscard]] const webgpu::raii::TextureView* result_view() const;

private:
    webgpu::Context* m_ctx = nullptr;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_pipeline;
    std::unique_ptr<webgpu::Framebuffer> m_atmosphere_framebuffer;
};

} // namespace webgpu_engine
