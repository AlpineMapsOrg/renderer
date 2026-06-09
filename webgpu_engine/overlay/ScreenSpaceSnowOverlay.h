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

#include "Overlay.h"
#include <memory>
#include <webgpu/Buffer.h>
#include <webgpu/raii/CombinedComputePipeline.h>
#include <webgpu/raii/TextureWithSampler.h>

namespace webgpu_engine {

class ScreenSpaceSnowOverlay : public Overlay {
public:
    struct Settings {
        float angle_min = 0.0f; // steepness lower limit (deg)
        float angle_max = 45.0f; // steepness upper limit (deg)
        float angle_blend = 5.0f; // band smoothing (deg)
        float altitude_limit = 1000.0f; // snow line altitude (m)
        float altitude_variation = 200.0f; // noise variation around the snow line (m)
        float altitude_blend = 200.0f; // altitude falloff (m)
        float transparency = 1.0f; // overlay opacity [0, 1]
        float _pad = 0.0f;
    };

    ScreenSpaceSnowOverlay();

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
