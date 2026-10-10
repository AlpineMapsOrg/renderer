/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include "SandboxView.h"

#include "ui/UiScale.h"
#include <algorithm>
#include <imgui.h>
#include <webgpu/compute/nodes/AvaFrameSandboxNode.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

void SandboxView::draw(const nodes::AvaFrameSandboxNode& sandbox, nodes::MpmSolverNode& solver)
{
    if (!open)
        return;
    const nodes::SandboxTerrain* terrain = sandbox.terrain();
    const auto* preview = sandbox.preview_texture();

    const ImVec2 viewport = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowSize(ImVec2(620 * ui::UI_SCALE, 620 * ui::UI_SCALE), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f - 310 * ui::UI_SCALE, 60 * ui::UI_SCALE), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AvaFrame sandbox", &open)) {
        ImGui::End();
        return;
    }
    if (terrain == nullptr || preview == nullptr) {
        ImGui::TextDisabled("No dataset loaded - pick one in the Avalanche section of the sidebar.");
        ImGui::End();
        return;
    }

    ImGui::Text("%s / %s", terrain->name.c_str(), terrain->scenario.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("t = %.1f s", double(solver.simulated_time()));
    ImGui::Checkbox("Particles", &m_show_particles);
    ImGui::SameLine();
    ImGui::Checkbox("Zoom to domain", &m_zoom_to_domain);
    ImGui::SameLine();
    ImGui::TextDisabled("cyan release, green com1DFA, white box domain");

    // Visible part of the DEM, in sandbox metres: everything, or the solver's domain.
    const auto& domain = solver.domain_aabb();
    const bool has_domain = domain.size().x > 0.0;
    glm::dvec2 view_min(0.0);
    glm::dvec2 view_max = terrain->size_metres;
    if (m_zoom_to_domain && has_domain) {
        view_min = domain.min;
        view_max = domain.max;
    }
    const glm::dvec2 view_size = view_max - view_min;

    // Fit the view into the remaining space, north up, square metres.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale = std::max(std::min(avail.x / float(view_size.x), avail.y / float(view_size.y)), 1e-4f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const auto to_screen = [&](glm::dvec2 metres) {
        return ImVec2(origin.x + float(metres.x - view_min.x) * scale, origin.y + float(view_max.y - metres.y) * scale);
    };
    const ImVec2 canvas_max(origin.x + float(view_size.x) * scale, origin.y + float(view_size.y) * scale);
    ImGui::InvisibleButton("##sandbox_canvas", ImVec2(canvas_max.x - origin.x, canvas_max.y - origin.y));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(origin, canvas_max, true);
    // DEM (row 0 = north = top of the image).
    draw_list->AddImage((ImTextureID)preview->texture_view().handle(), to_screen(glm::dvec2(0.0, terrain->size_metres.y)),
        to_screen(glm::dvec2(terrain->size_metres.x, 0.0)));

    // Particle raster over the domain (also row 0 = north).
    if (has_domain) {
        const ImVec2 top_left = to_screen(glm::dvec2(domain.min.x, domain.max.y));
        const ImVec2 bottom_right = to_screen(glm::dvec2(domain.max.x, domain.min.y));
        const auto output = solver.output_socket("texture").get_data();
        const auto* texture = std::get<nodes::data_type<const webgpu::raii::TextureWithSampler*>()>(output);
        if (m_show_particles && texture != nullptr)
            draw_list->AddImage((ImTextureID)texture->texture_view().handle(), top_left, bottom_right);
        draw_list->AddRect(top_left, bottom_right, IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
    }
    draw_list->PopClipRect();

    // Hover readout: position and terrain height.
    if (ImGui::IsItemHovered()) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const glm::dvec2 metres(view_min.x + double(mouse.x - origin.x) / scale, view_max.y - double(mouse.y - origin.y) / scale);
        const glm::ivec2 cell(int(metres.x / terrain->cellsize), int(terrain->grid_size.y) - 1 - int(metres.y / terrain->cellsize));
        if (cell.x >= 0 && cell.y >= 0 && cell.x < int(terrain->grid_size.x) && cell.y < int(terrain->grid_size.y)) {
            const glm::uvec2 c(cell);
            ImGui::SetTooltip("%.0f, %.0f m\nheight %.0f m%s\nrelease depth %.2f m", metres.x, metres.y, double(terrain->heights.pixel(c)),
                terrain->nodata.pixel(c) ? " (NODATA, filled)" : "", double(terrain->release_depth.pixel(c)));
        }
    }

    ImGui::End();
}

} // namespace webgpu_app
