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

#include "CompassPanel.h"

#include "../ImGuiManager.h"
#include <imgui.h>
#include <imgui_internal.h>

#include "../TerrainRenderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace webgpu_app {

CompassPanel::CompassPanel(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
}

void CompassPanel::draw()
{
    // === ROTATE NORTH BUTTON ===
    // Claim the next floating tool-button slot (bottom-left, stacking upward).
    ImVec2 button_pos(10, ImGuiManager::s_tool_button_y);
    ImGuiManager::s_tool_button_y -= 48 + 10;
    ImGui::SetNextWindowPos(button_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f); // Semi-transparent background
    ImGui::SetNextWindowSize(ImVec2(48, 48)); // Set button size

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // No padding for better icon alignment
    ImGui::Begin("RotateNorthButton", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // fully transparent
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // black with alpha 0.2
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.2f)); // same for active

    auto camController = m_terrain_renderer->get_camera_controller();

    if (ImGui::Button("###RotateNorthButton", ImVec2(48, 48))) {
        camController->rotate_north();
    }
    const bool hovered = ImGui::IsItemHovered();

    ImGui::PopStyleColor(3);

    // Drawing the arrow icon manually with rotation
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const auto rectMin = ImGui::GetItemRectMin();

    auto cameraFrontAxis = camController->definition().z_axis();
    auto degFromNorth = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::dvec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::dvec3(0, -1, 0))));
    float cameraAngle = cameraFrontAxis.x > 0 ? degFromNorth : -degFromNorth;

    ImVec2 center = ImVec2(rectMin.x + 24, rectMin.y + 24); // Center of the button
    float rotation_angle = cameraAngle * (glm::pi<float>() / 180.0f);

    // Define arrow vertices relative to the center
    float arrow_length = 16.0f; // Size of the arrow
    ImVec2 points[3] = {
        ImVec2(0.0f, -arrow_length), // Arrow tip
        ImVec2(-arrow_length * 0.5f, arrow_length * 0.5f), // Left base
        ImVec2(arrow_length * 0.5f, arrow_length * 0.5f), // Right base
    };

    // Rotate and translate arrow vertices to draw at the specified angle
    for (int i = 0; i < 3; ++i) {
        float rotated_x = cos(rotation_angle) * points[i].x - sin(rotation_angle) * points[i].y;
        float rotated_y = sin(rotation_angle) * points[i].x + cos(rotation_angle) * points[i].y;
        points[i] = ImVec2(center.x + rotated_x, center.y + rotated_y);
    }

    // Draw the rotated arrow
    draw_list->AddTriangleFilled(points[0], points[1], points[2], IM_COL32(255, 255, 255, 255)); // White color for the arrow

    ImGui::End();
    ImGui::PopStyleVar(); // Restore padding

    if (hovered)
        ImGui::SetTooltip("Rotate North");
}

} // namespace webgpu_app
