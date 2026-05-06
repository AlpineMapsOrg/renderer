/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "TimingPanel.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>

#include "../TerrainRenderer.h"

namespace webgpu_app {

TimingPanel::TimingPanel(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
}

void TimingPanel::draw_panel()
{
    if (ImGui::CollapsingHeader(ICON_FA_STOPWATCH "  Timing")) {

        const webgpu::timing::GuiTimerWrapper* selected_timer = nullptr;
        if (!m_selected_timer.empty()) {
            uint32_t first_selected_timer_id = *m_selected_timer.begin();
            selected_timer = m_terrain_renderer->get_timer_manager()->get_timer_by_id(first_selected_timer_id);
        }
        if (selected_timer) {
            const auto* tmr = selected_timer->timer.get();
            if (tmr->get_sample_count() > 2) {
                ImVec4 timer_color = *(ImVec4*)(void*)&selected_timer->color;
                ImGui::PushStyleColor(ImGuiCol_PlotLines, timer_color);
                ImGui::PlotLines("##SelTimerGraph", &tmr->get_results()[0], (int)tmr->get_sample_count(), 0, nullptr, 0.0f, tmr->get_max(), ImVec2(380, 80));
                ImGui::PopStyleColor();
            }
        }

        auto group_list = m_terrain_renderer->get_timer_manager()->get_groups();
        for (const auto& group : group_list) {
            bool showGroup = true;
            if (group.name != "") {
                ImGui::Indent();
                showGroup = ImGui::CollapsingHeader(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            }
            if (showGroup) {
                for (const auto& tmr : group.timers) {
                    const uint32_t tmr_id = tmr.timer->get_id();
                    ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
                    if (is_timer_selected(tmr_id)) {
                        color = *(ImVec4*)(void*)&tmr.color;
                    }

                    if (ImGui::ColorButton(
                            ("##t" + std::to_string(tmr_id)).c_str(), color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(10, 10))) {
                        toggle_timer(tmr_id);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s: %s ±%s [%zu]", tmr.name.c_str(), webgpu::timing::format_time(tmr.timer->get_average()).c_str(),
                        webgpu::timing::format_time(tmr.timer->get_standard_deviation()).c_str(), tmr.timer->get_sample_count());
                }
            }
            if (group.name != "")
                ImGui::Unindent();
        }
        if (ImGui::Button("Reset All Timers")) {
            for (const auto& group : group_list)
                for (const auto& tmr : group.timers)
                    tmr.timer->clear_results();
        }
    }
}

void TimingPanel::toggle_timer(uint32_t timer_id)
{
    if (is_timer_selected(timer_id)) {
        m_selected_timer.erase(timer_id);
    } else {
        m_selected_timer.clear(); // Remove if multiple selection possible
        m_selected_timer.insert(timer_id);
    }
}

bool TimingPanel::is_timer_selected(uint32_t timer_id) const
{
    return m_selected_timer.find(timer_id) != m_selected_timer.end();
}

} // namespace webgpu_app
