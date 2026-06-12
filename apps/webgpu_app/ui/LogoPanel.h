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

#include "ImGuiPanel.h"
#include <imgui.h>
#include <memory>
#include <webgpu/webgpu.h>

namespace webgpu::raii {
class Texture;
class TextureView;
} // namespace webgpu::raii

namespace webgpu_app {

class LogoPanel : public ImGuiPanel {
public:
    explicit LogoPanel(WGPUDevice device);

    void draw() override;

private:
    WGPUDevice m_device;

    ImVec2 m_webigeo_logo_size = {};
    std::unique_ptr<webgpu::raii::Texture> m_webigeo_logo;
    std::unique_ptr<webgpu::raii::TextureView> m_webigeo_logo_view;

    void init_logo();
};

} // namespace webgpu_app
