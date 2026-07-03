/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Patrick Komon
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

#include "SearchPanel.h"

#include "App.h"
#include "IconsFontAwesome5.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

namespace webgpu_app {

SearchPanel::SearchPanel(App* renderer)
    : m_terrain_renderer(renderer)
{
}

void SearchPanel::draw()
{
    const size_t max_num_search_results_at_once = 10;
    const int window_height = ImGui::GetTextLineHeightWithSpacing() + 2 * ImGui::GetStyle().WindowPadding.y + 2 * 3
        + std::min(m_search_results.size(), max_num_search_results_at_once) * ImGui::GetTextLineHeightWithSpacing() + (m_search_results.empty() ? 0 : 12)
        + (m_show_no_results ? ImGui::GetTextLineHeightWithSpacing() : 0);
    const int search_button_width = 30;
    const int search_text_width = 340;
    const int window_width = search_text_width + search_button_width + 2 * ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().ItemSpacing.x;

    ImVec2 window_pos((ImGui::GetIO().DisplaySize.x - 215) / 2 - window_width / 2, 30);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_is_active ? 1.0f : 0.4f);
    if (ImGui::Begin(
            "search_panel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
        m_is_active = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(78 / 255.0f, 163 / 255.0f, 196 / 255.0f, 1.00f));
        ImGui::PushItemWidth(search_text_width);

        // NOTE: The following is necessary to clear some form of imgui cache which leads to
        // reseting of the input buffer not properly displayed
        if (m_clear_input_requested) {
            m_clear_input_requested = false;
            ImGui::ClearActiveID();
        }

        if (m_set_focus_on_text) {
            m_set_focus_on_text = false;
            ImGui::SetKeyboardFocusHere();
        }
        if (ImGui::InputText("##search_input", m_search_buffer.data(), m_search_buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            qDebug() << "search requested" << m_search_buffer.data();
            m_show_no_results = false;
            m_search_results.clear();
            emit search_requested(std::string(m_search_buffer.data()));
            m_set_focus_on_text = true;
        }

        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SEARCH, ImVec2(search_button_width, 0))) {
            qDebug() << "search requested" << m_search_buffer.data();
            m_show_no_results = false;
            m_search_results.clear();
            emit search_requested(std::string(m_search_buffer.data()));
        }

        if (m_show_no_results) {
            ImGui::TextDisabled("No results found.");
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
                        select_result(m_search_results.at(i));
                    }

                    if (ImGui::IsItemHovered()) {
                        item_highlighted_idx = i;
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void SearchPanel::select_result(const SearchResult& result)
{
    qDebug() << "result selected" << result.name;
    emit search_result_selected(result.latitude, result.longitude);
    m_search_buffer.fill('\0');
    m_search_results.clear();
    m_show_no_results = false;
    m_clear_input_requested = true;
    m_is_active = false;
}

void SearchPanel::display_search_results(const std::vector<SearchResult>& search_results)
{
    if (search_results.size() == 1) {
        select_result(search_results[0]);
        return;
    }
    m_show_no_results = search_results.empty();
    m_search_results = search_results;
}

} // namespace webgpu_app
