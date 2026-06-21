/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "LogoPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <nucleus/utils/image_loader.h>
#include <webgpu/base/raii/Texture.h>

namespace webgpu_app {

LogoPanel::LogoPanel(WGPUDevice device)
    : m_device(device)
{
    init_logo();
}

void LogoPanel::init_logo()
{
    nucleus::Raster<glm::u8vec4> logo = nucleus::utils::image_loader::rgba8(":/gfx/sujet.png").value();
    m_webigeo_logo_size = ImVec2(float(logo.width()), float(logo.height()));

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "webigeo logo texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { uint32_t(logo.width()), uint32_t(logo.height()), uint32_t(1) };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm;
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    m_webigeo_logo = std::make_unique<webgpu::raii::Texture>(m_device, texture_desc);
    auto queue = wgpuDeviceGetQueue(m_device);
    m_webigeo_logo->write(queue, logo);
    m_webigeo_logo_view = m_webigeo_logo->create_view();
}

void LogoPanel::draw()
{ // weBIGeo LOGO
    ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    float viewportWidth = viewportSize.x;
    const float minWidth = 800.0f;
    const float maxWidth = 1920.0f;

    float scaleFactor = 1.0f;
    if (viewportWidth <= minWidth) {
        scaleFactor = 0.5f;
    } else if (viewportWidth >= maxWidth) {
        scaleFactor = 1.0f;
    } else {
        scaleFactor = 0.5f + 0.5f * ((viewportWidth - minWidth) / (maxWidth - minWidth));
    }
    ImVec2 scaledSize = ImVec2(m_webigeo_logo_size.x * scaleFactor * 0.75f, m_webigeo_logo_size.y * scaleFactor * 0.75f);
    const float padX = 20.0f;
    const float padY = 10.0f;
    const float offsetY = 30.0f;
    const float topRightSkew = 50.0f;

    float bgRight = padX + scaledSize.x + padX;
    float bgBottom = offsetY + padY + scaledSize.y + padY;
    ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg, 0.5f);
    ImGui::GetBackgroundDrawList()->AddQuadFilled(
        ImVec2(0.0f, offsetY),
        ImVec2(bgRight + topRightSkew, offsetY),
        ImVec2(bgRight, bgBottom),
        ImVec2(0.0f, bgBottom),
        bgColor);

    ImGui::SetNextWindowPos(ImVec2(0, offsetY), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padX, padY));
    ImGui::Begin("weBIGeo-Logo",
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());

    ImGui::Image((ImTextureID)m_webigeo_logo_view->handle(), scaledSize);
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace webgpu_app
