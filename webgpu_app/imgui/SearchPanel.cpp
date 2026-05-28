/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Patrick Komon
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

#include "SearchPanel.h"

#include "IconsFontAwesome5.h"
#include <TerrainRenderer.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

namespace webgpu_app {

SearchPanel::SearchPanel(TerrainRenderer* renderer)
    : m_terrain_renderer(renderer)
{
}

void SearchPanel::draw()
{
    draw_open_search_button();
    draw_search_popup();
}

void SearchPanel::draw_open_search_button()
{
    ImVec2 button_pos(10, ImGui::GetIO().DisplaySize.y - 3 * 48 - 40 - 10 * 2);
    ImGui::SetNextWindowPos(button_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::SetNextWindowSize(ImVec2(48, 48));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("ToggleSearchWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.2f));

    if (ImGui::Button(ICON_FA_SEARCH "###ToggleSearchWindow", ImVec2(48, 48))) {
        m_open_search_window = !m_open_search_window;
        m_set_focus_on_text = m_open_search_window;
    }

    ImGui::PopStyleColor(3);
    ImGui::End();
    ImGui::PopStyleVar();
}

void SearchPanel::draw_search_popup()
{
    if (m_open_search_window) {
        m_open_search_window = false;
        ImGui::OpenPopup("search_panel");
    }

    const size_t max_num_search_results_at_once = 10; // if more search result, adds scroll bar
    const int line_height = ImGui::GetTextLineHeightWithSpacing();
    const int window_height = ImGui::GetTextLineHeightWithSpacing() + 2 * ImGui::GetStyle().WindowPadding.y + 2 * 3
        + std::min(m_search_results.size(), max_num_search_results_at_once) * ImGui::GetTextLineHeightWithSpacing() + (m_search_results.empty() ? 0 : 12);
    const int search_button_width = 60;
    const int search_text_width = 340;
    const int window_width = search_text_width + search_button_width + 2 * ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ItemSpacing.x;

    ImVec2 window_pos(ImGui::GetIO().DisplaySize.x / 2 - window_width / 2, line_height);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    if (ImGui::BeginPopup("search_panel", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f));
        ImGui::PushItemWidth(search_text_width);

        static std::array<char, 128> buffer {};

        if (m_set_focus_on_text) {
            m_set_focus_on_text = false;
            ImGui::SetKeyboardFocusHere();
        }
        if (ImGui::InputText("##search_input", buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            qInfo() << "search requested" << buffer.data();
            emit search_requested(std::string(buffer.data()));
            m_set_focus_on_text = true;
        }

        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Search", ImVec2(search_button_width, 0))) {
            qInfo() << "search requested" << buffer.data();
            emit search_requested(std::string(buffer.data()));
        }

        if (!m_search_results.empty()) {
            int item_selected_idx = -1;
            int item_highlighted_idx = -1;

            if (ImGui::BeginListBox("##search_results",
                    ImVec2(-FLT_MIN,
                        std::min(m_search_results.size(), max_num_search_results_at_once) * ImGui::GetTextLineHeightWithSpacing()
                            + 2 * ImGui::GetStyle().ItemInnerSpacing.y))) {
                for (int i = 0; i < int(m_search_results.size()); i++) {
                    bool is_selected = (item_selected_idx == i);
                    ImGuiSelectableFlags flags = (item_highlighted_idx == i) ? ImGuiSelectableFlags_Highlight : 0;
                    if (ImGui::Selectable(m_search_results.at(i).name.c_str(), is_selected, flags)) {
                        item_selected_idx = i;
                        qInfo() << "result selected" << m_search_results.at(i).name;
                        emit search_result_selected(m_search_results.at(i).latitude, m_search_results.at(i).longitude);

                        // clear text and search results
                        ImGui::CloseCurrentPopup();
                        buffer.fill('\0');
                        m_search_results.clear();
                    }

                    if (ImGui::IsItemHovered()) {
                        item_highlighted_idx = i;
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

void SearchPanel::display_search_results(const std::vector<SearchResult>& search_results) { m_search_results = search_results; }

} // namespace webgpu_app
