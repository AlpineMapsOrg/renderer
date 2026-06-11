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
#include "overlays/OverlayImGuiRenderer.h"
#include <imgui.h>
#include <memory>
#include <vector>

namespace webgpu_engine {
class Context;
class OverlayRenderer;
} // namespace webgpu_engine

namespace webgpu_app {

class OverlaysPanel : public ImGuiPanel {
public:
    explicit OverlaysPanel(webgpu_engine::Context* context);
    void draw_panel() override;
    void draw() override;

private:
    void rebuild_renderers();
    void do_move(int gui_row, int direction, int Q, int N);

    void draw_add_overlay_popup();
    void add_overlay_of_type(int type);

    webgpu_engine::Context* m_context;
    webgpu_engine::OverlayRenderer* m_overlay_renderer = nullptr;
    std::vector<std::unique_ptr<OverlayImGuiRenderer>> m_renderers;
    int m_selected_engine_idx = -1; // engine index of selected overlay (-1 = none)
    float m_selected_row_screen_y = 0.0f; // screen Y of selected row (for settings popup)
    int m_add_type_index = 0;
    bool m_add_popup_modal = false; // true: button (modal); false: Shift+A (non-modal at cursor)
    ImVec2 m_add_popup_pos {};
    bool m_open_chooser_request = false;
};

} // namespace webgpu_app
