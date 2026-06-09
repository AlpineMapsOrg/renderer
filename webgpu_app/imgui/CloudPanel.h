/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2026 Wendelin Muth
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

namespace webgpu_engine {
class CloudRenderer;
class Context;
} // namespace webgpu_engine

namespace webgpu_app {

namespace clouds {
    class Manager;
}

class CloudPanel : public ImGuiPanel {
public:
    CloudPanel(webgpu_engine::Context* context, clouds::Manager* clouds_manager, webgpu_engine::CloudRenderer* cloud_renderer);

    void draw() override;
    void draw_panel() override;

private:
    webgpu_engine::Context* m_context;
    clouds::Manager* m_clouds_manager;
    webgpu_engine::CloudRenderer* m_cloud_renderer;
};

} // namespace webgpu_app
