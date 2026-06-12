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

#include "AboutPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace webgpu_app {

void AboutPanel::ready()
{
#ifndef QT_DEBUG
    m_open_disclaimer = true;
#endif
}

void AboutPanel::draw()
{
    if (m_open_disclaimer) {
        ImGui::OpenPopup("research_preview");
        m_open_disclaimer = false;
    }
    draw_disclaimer_popup();
    draw_copyright_box();
    draw_about_popup();
}

void AboutPanel::draw_copyright_box()
{ // Draw the copyright box
    // Position the white box in the bottom-left corner
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 30), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f); // Semi-transparent background
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4)); // Reduce padding
    // Set border color to transparent
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent border
    ImGui::Begin("CopyrightBox", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    // Set up a button with no hover effect by temporarily changing colors
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f)); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 1.0f, 0.2f)); // No hover effect
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 1.0f, 0.1f)); // No active effect

    if (ImGui::Button("About")) {
        m_show_about_popup = true; // necessary due to ImGui-Window-Scope
    }

    ImGui::PopStyleColor(3);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(); // Restore padding
}

void AboutPanel::draw_about_popup()
{
    if (m_show_about_popup) {
        m_show_about_popup = false;
        ImGui::OpenPopup("about_webigeo");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 450, 478 });

    if (ImGui::BeginPopupModal("about_webigeo", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        const char* title = "About weBIGeo";
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::Text("%s", title);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextWrapped("weBIGeo is a research project to show that processing and "
                           "visualizing of large datasets is possible in the browser near real-time.");

        ImGui::Spacing();
        ImGui::Text("This project is based on ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("AlpineMaps.org", "https://github.com/AlpineMapsOrg/renderer");

        ImGui::Spacing();
        ImGui::Text("It is licensed under the ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("GPLv3", "https://www.gnu.org/licenses/gpl-3.0.html#license-text");

        ImGui::Spacing();
        ImGui::TextLinkOpenURL("GitHub repository", "https://github.com/weBIGeo/webigeo");
        ImGui::TextLinkOpenURL("netidee project page", "https://www.netidee.at/webigeo");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Authors:");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Adam Celarek", "https://github.com/adam-ce");
        ImGui::SameLine();
        ImGui::Text("(2022-2025)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Gerald Kimmersdorfer", "https://github.com/GeraldKimmersdorfer");
        ImGui::SameLine();
        ImGui::Text("(2023-2026)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Jakob Lindner", "https://github.com/JakobLindner");
        ImGui::SameLine();
        ImGui::Text("(2023)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Patrick Komon", "https://github.com/pkomon");
        ImGui::SameLine();
        ImGui::Text("(2024-2026)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Jakob Maier", "https://github.com/Gro2mi");
        ImGui::SameLine();
        ImGui::Text("(2024)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Markus Rampp", "https://github.com/gue-ni");
        ImGui::SameLine();
        ImGui::Text("(2025)");
        ImGui::Text(" - ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Wendelin Muth", "https://github.com/Qendolin");
        ImGui::SameLine();
        ImGui::Text("(2026)");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Height and ortho data is provided by ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("basemap.at", "https://basemap.at/");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextWrapped("If you have feedback or ideas for collaboration, contact us!");
        ImGui::Text("E-Mail: ");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("alpinemaps@cg.tuwien.ac.at", "mailto:alpinemaps@cg.tuwien.ac.at");
        ImGui::TextLinkOpenURL("Join us on Discord", "https://discord.gg/j4MRrrbh");

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::SetCursorPosX(windowWidth - 200 - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Close", { 200, 0 })) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AboutPanel::draw_disclaimer_popup()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(365, 208));

    if (ImGui::BeginPopupModal("research_preview", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        const char* text = "WARNING: RESEARCH PREVIEW";
        auto windowWidth = ImGui::GetWindowSize().x;
        auto textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::Text("%s", text);

        ImGui::Spacing();
        ImGui::TextWrapped(
            "The avalanche simulation and visualization is part of a research project and should not be used as a basis for decision-making during "
            "actual route planning.");
        ImGui::Spacing();
        ImGui::TextWrapped("Simulations always contain uncertainty and their results\n"
                           "may differ drastically from reality in some cases.");
        ImGui::Spacing();
        ImGui::TextWrapped("We exclude liability for any accidents or damages in connection to this service.");
        ImGui::Spacing();
        ImGui::SetCursorPosX(205);
        if (ImGui::Button("OK", ImVec2(150, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace webgpu_app
