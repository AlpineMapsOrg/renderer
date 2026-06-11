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

#include "OverlaysPanel.h"

#include "overlays/OverlayImGuiRendererFactory.h"

#include <IconsFontAwesome5.h>
#include <algorithm>
#include <imgui.h>

#include <webgpu_engine/Context.h>
#include <webgpu_engine/overlay/HeightLinesOverlay.h>
#include <webgpu_engine/overlay/OverlayRenderer.h>
#include <webgpu_engine/overlay/ScreenSpaceSnowOverlay.h>
#include <webgpu_engine/overlay/TextureOverlay.h>
#include <webgpu_engine/overlay/TileDebugOverlay.h>

namespace webgpu_app {

OverlaysPanel::OverlaysPanel(webgpu_engine::Context* context)
    : m_context(context)
    , m_overlay_renderer(context->overlay_renderer())
{
    rebuild_renderers();
}

void OverlaysPanel::rebuild_renderers()
{
    m_renderers.clear();
    for (auto& overlay : m_overlay_renderer->overlays())
        m_renderers.push_back(OverlayImGuiRendererFactory::create(*overlay));
    if (m_selected_engine_idx >= static_cast<int>(m_renderers.size()))
        m_selected_engine_idx = -1;
}

// GUI shows overlays in descending z_index order with a virtual shading row at position P

static int display_to_engine(int d, int N) { return N - 1 - d; }

void OverlaysPanel::do_move(int gui_row, int direction, int P, int N)
{
    const int other = gui_row + direction;
    if (other < 0 || other > N)
        return;

    m_selected_engine_idx = -1;

    auto gui_slots = m_overlay_renderer->overlays(); // ascending by z_index
    std::reverse(gui_slots.begin(), gui_slots.end()); // descending: post-shading first, then pre-shading
    gui_slots.insert(gui_slots.begin() + P, nullptr); // divider at slot P -> size becomes N+1

    std::swap(gui_slots[gui_row], gui_slots[other]);

    int new_P = 0;
    for (int i = 0; i < static_cast<int>(gui_slots.size()); ++i)
        if (!gui_slots[i]) {
            new_P = i;
            break;
        }

    for (int i = 0; i < static_cast<int>(gui_slots.size()); ++i) {
        if (i == new_P)
            continue;
        if (i < new_P)
            gui_slots[i]->z_index = new_P - i;
        else
            gui_slots[i]->z_index = -(i - new_P);
    }

    m_overlay_renderer->sort_overlays(); // re-sort m_overlays by the updated z_indices

    rebuild_renderers();
    m_context->request_redraw();
}

namespace {
    enum AddType { ADD_HEIGHT_LINES = 0, ADD_TEXTURE_OVERLAY = 1, ADD_TILE_DEBUG = 2, ADD_SCREEN_SPACE_SNOW = 3 };
    constexpr const char* ADD_ITEMS[] = { "Height Lines", "Texture Overlay", "Tile Debug", "Screen-Space Snow" };
    constexpr const char* ADD_POPUP_ID = "Add Overlay###add_overlay";
} // namespace

void OverlaysPanel::add_overlay_of_type(int type)
{
    std::shared_ptr<webgpu_engine::Overlay> new_overlay;
    if (type == ADD_HEIGHT_LINES)
        new_overlay = std::make_shared<webgpu_engine::HeightLinesOverlay>();
    else if (type == ADD_TEXTURE_OVERLAY)
        new_overlay = std::make_shared<webgpu_engine::TextureOverlay>();
    else if (type == ADD_SCREEN_SPACE_SNOW)
        new_overlay = std::make_shared<webgpu_engine::ScreenSpaceSnowOverlay>();
    else
        new_overlay = std::make_shared<webgpu_engine::TileDebugOverlay>();
    // add_overlay() auto-assigns the topmost z_index
    m_overlay_renderer->add_overlay(new_overlay);
    rebuild_renderers();
    m_selected_engine_idx = static_cast<int>(m_overlay_renderer->overlays().size()) - 1;
    m_context->request_redraw();
}

void OverlaysPanel::draw_add_overlay_popup()
{
    if (!m_add_popup_modal)
        ImGui::SetNextWindowPos(m_add_popup_pos, ImGuiCond_Appearing);

    const bool open = m_add_popup_modal ? ImGui::BeginPopupModal(ADD_POPUP_ID, nullptr, ImGuiWindowFlags_AlwaysAutoResize) : ImGui::BeginPopup(ADD_POPUP_ID);
    if (!open)
        return;

    if (!m_add_popup_modal)
        ImGui::TextDisabled("Add Overlay");

    ImGui::SetNextItemWidth(200.0f);
    ImGui::Combo("##add_type", &m_add_type_index, ADD_ITEMS, IM_ARRAYSIZE(ADD_ITEMS));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        add_overlay_of_type(m_add_type_index);
        ImGui::CloseCurrentPopup();
    }
    if (m_add_popup_modal) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void OverlaysPanel::draw_panel()
{
    if (!m_overlay_renderer)
        return;

    // Overlays can be added/removed outside the panel (e.g. by a compute OverlayNode), so we check if the list converges:
    if (m_renderers.size() != m_overlay_renderer->overlays().size())
        rebuild_renderers();

    if (!ImGui::CollapsingHeader(ICON_FA_LAYER_GROUP "  Overlays", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (ImGui::Button(ICON_FA_PLUS "  Add Overlay [Shift + A]", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        m_add_popup_modal = true;
        m_open_chooser_request = true;
    }

    ImGui::Separator();

    // OVERLAY LIST ====
    const auto& overlays = m_overlay_renderer->overlays();
    const int N = static_cast<int>(overlays.size());

    const float row_h = ImGui::GetTextLineHeightWithSpacing() + 6.0f;

    if (N == 0) {
        ImGui::Selectable("No overlay configured", false, ImGuiSelectableFlags_Disabled, ImVec2(0.0, row_h));
        return;
    }

    int P = 0; // count of post-shading overlays (z_index > 0)
    for (const auto& o : overlays)
        if (o->z_index > 0)
            ++P;

    const float btn_w = row_h;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float btns_w = btn_w * 3.0f + spacing * 2.0f;

    // Pending actions, applied after the loop (mutating the list mid-iteration is unsafe).
    int move_row = -1, move_dir = 0;
    int delete_engine_idx = -1;

    // There are N+1 GUI rows total (N overlays + the divider)
    for (int gui_row = 0; gui_row <= N; ++gui_row) {

        // SHADING DIVIDER
        if (gui_row == P) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.82f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
            ImGui::Selectable(
                "---------- " ICON_FA_SUN "  SHADING ----------", false, ImGuiSelectableFlags_Disabled, ImVec2(ImGui::GetContentRegionAvail().x, row_h));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            continue;
        }

        // OVERLAY ENTRY
        {
            const int d = (gui_row < P) ? gui_row : gui_row - 1;
            const int engine_idx = display_to_engine(d, N);
            ImGui::PushID(engine_idx);

            const bool selected = (m_selected_engine_idx == engine_idx);
            const std::string name = (engine_idx < static_cast<int>(m_renderers.size())) ? m_renderers[engine_idx]->effective_name() : "Overlay";

            if (selected)
                m_selected_row_screen_y = ImGui::GetCursorScreenPos().y;

            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - btns_w - spacing, row_h)))
                m_selected_engine_idx = selected ? -1 : engine_idx;
            ImGui::PopStyleVar();
            ImGui::SameLine();

            const bool can_up = (gui_row > 0);
            const bool can_down = (gui_row < N);

            // MOVE UP BUTTON
            ImGui::BeginDisabled(!can_up);
            if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2(btn_w, row_h))) {
                move_row = gui_row;
                move_dir = -1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            // MOVE DOWN BUTTON
            ImGui::BeginDisabled(!can_down);
            if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(btn_w, row_h))) {
                move_row = gui_row;
                move_dir = +1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            // DELETE BUTTON
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH, ImVec2(btn_w, row_h)))
                delete_engine_idx = engine_idx;
            ImGui::PopStyleColor(2);

            ImGui::PopID();
        }
    }

    // Apply the action defered here, such that we dont mutate the list while rendering
    if (move_dir != 0) {
        do_move(move_row, move_dir, P, N);
    } else if (delete_engine_idx >= 0) {
        if (m_selected_engine_idx == delete_engine_idx)
            m_selected_engine_idx = -1;
        m_overlay_renderer->remove_overlay(static_cast<size_t>(delete_engine_idx));
        rebuild_renderers();
        m_context->request_redraw();
    }
}

void OverlaysPanel::draw()
{
    if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        m_add_popup_modal = false;
        m_add_popup_pos = ImGui::GetMousePos();
        m_open_chooser_request = true;
    }
    // NOTE: OpenPopup must share scope with BeginPopup* below, so it lives here rather than in draw_panel().
    if (m_open_chooser_request) {
        ImGui::OpenPopup(ADD_POPUP_ID);
        m_open_chooser_request = false;
    }
    draw_add_overlay_popup();

    if (m_overlay_renderer && m_selected_engine_idx >= 0 && m_selected_engine_idx < static_cast<int>(m_renderers.size())) {
        auto& renderer = m_renderers[m_selected_engine_idx];

        const float popup_w = 430.0f;
        const float sidebar_x = ImGui::GetIO().DisplaySize.x - 430.0f;
        ImGui::SetNextWindowPos(ImVec2(sidebar_x - popup_w - 8.0f, m_selected_row_screen_y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(popup_w, 0.0f));

        bool open = true;
        const std::string title = renderer->effective_name() + "###overlay_settings";
        ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoSavedSettings);

        if (renderer->render_settings())
            m_context->request_redraw();

        ImGui::End();

        if (!open)
            m_selected_engine_idx = -1;
    }
}

} // namespace webgpu_app
