/*****************************************************************************
 * weBIGeo
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

#include "Overlay.h"
#include <memory>
#include <webgpu/base/Buffer.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/TextureWithSampler.h>

namespace webgpu_engine {

class HeightLinesOverlay : public Overlay {
public:
    struct Settings {
        float primary_interval = 250.0f; // major contour interval in meters
        float secondary_interval = 50.0f; // minor contour interval in meters
        float base_width = 2.0f; // line base width
        float minor_opacity = 0.75f; // minor line opacity relative to major
        glm::vec4 line_color = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f); // color of the height lines
    };

    HeightLinesOverlay();

    void init(Context& ctx) override;
    void update_settings();
    void draw(const WGPUCommandEncoder& command_encoder,
        const webgpu::raii::TextureView& position_view,
        const webgpu::raii::TextureView& normal_view,
        const webgpu::raii::TextureView& overlay_view,
        const WGPUBindGroup& shared_config_bg,
        const WGPUBindGroup& camera_bg,
        const webgpu::raii::TextureWithSampler& current_input,
        webgpu::raii::TextureWithSampler& target_output,
        glm::uvec2 output_size) override;

    Settings settings;

private:
    webgpu::Context* m_ctx = nullptr;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_pipeline;
    std::unique_ptr<webgpu::Buffer<Settings>> m_settings_uniform;
};

} // namespace webgpu_engine
