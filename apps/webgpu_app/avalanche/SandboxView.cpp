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
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <webgpu/compute/nodes/AvaFrameSandboxNode.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

namespace {
    bool is_mpm_peak(SandboxView::Layer layer)
    {
        return layer == SandboxView::Layer::MpmThickness || layer == SandboxView::Layer::MpmVelocity || layer == SandboxView::Layer::MpmPressure;
    }
    bool is_reference(SandboxView::Layer layer)
    {
        return layer == SandboxView::Layer::RefThickness || layer == SandboxView::Layer::RefVelocity || layer == SandboxView::Layer::RefPressure;
    }
    nodes::SandboxQuantity quantity_of(SandboxView::Layer layer)
    {
        switch (layer) {
        case SandboxView::Layer::MpmVelocity:
        case SandboxView::Layer::RefVelocity:
            return nodes::SandboxQuantity::Velocity;
        case SandboxView::Layer::MpmPressure:
        case SandboxView::Layer::RefPressure:
            return nodes::SandboxQuantity::Pressure;
        default:
            return nodes::SandboxQuantity::Thickness;
        }
    }
    const std::vector<float>& values_of(const nodes::SandboxPeaks& peaks, nodes::SandboxQuantity q)
    {
        return q == nodes::SandboxQuantity::Velocity ? peaks.pfv : q == nodes::SandboxQuantity::Pressure ? peaks.ppr : peaks.pft;
    }
    const std::vector<float>& reference_of(const nodes::SandboxTerrain& t, nodes::SandboxQuantity q)
    {
        return q == nodes::SandboxQuantity::Velocity ? t.reference_pfv : q == nodes::SandboxQuantity::Pressure ? t.reference_ppr : t.reference_pft;
    }
    ImU32 to_imgui(glm::u8vec4 c) { return IM_COL32(c.x, c.y, c.z, 255); }
} // namespace

SandboxView::SandboxView()
    : m_alive(std::make_shared<bool>(true))
{
}

SandboxView::~SandboxView() { *m_alive = false; }

void SandboxView::request_peaks(nodes::AvaFrameSandboxNode& sandbox, nodes::MpmSolverNode& solver)
{
    const nodes::SandboxTerrain* terrain = sandbox.terrain();
    if (terrain == nullptr || m_peaks_pending)
        return;
    const auto now = std::chrono::steady_clock::now();
    const std::string key = terrain->dataset_path + "/" + terrain->scenario;
    const bool stale = key != m_peaks_key || solver.simulated_time() != m_peaks_time;
    if (!stale || now - m_last_request < std::chrono::milliseconds(800))
        return;
    m_last_request = now;
    m_peaks_pending = true;
    const float density = solver.get_settings().snow_density;
    const std::weak_ptr<bool> alive = m_alive;
    const std::weak_ptr<bool> node_alive = sandbox.alive_token();
    auto* self = this;
    auto* node = &sandbox;
    // Fires only while the solver lives. The view and the sandbox node have their own tokens,
    // and the dataset may have changed meanwhile, hence the key.
    solver.read_back_peaks([alive, node_alive, self, node, key, density](const nodes::MpmSolverNode::ColumnPeaks& columns) {
        const auto view = alive.lock();
        if (!view || !*view)
            return;
        self->m_peaks_pending = false;
        const auto node_ok = node_alive.lock();
        if (!node_ok || !*node_ok)
            return;
        const nodes::SandboxTerrain* t = node->terrain();
        if (t == nullptr || t->dataset_path + "/" + t->scenario != key)
            return;
        self->m_peaks = std::make_shared<nodes::SandboxPeaks>(nodes::resample_sandbox_peaks(*t, columns, density));
        self->m_peaks_key = key;
        self->m_peaks_time = columns.simulated_time;
        self->m_layer_dirty = true;
        // Real datasets: the map overlay follows the same peaks.
        node->update_map_overlay(&self->m_peaks->pft);
    });
}

void SandboxView::draw_legend(Layer layer)
{
    if (layer == Layer::Particles)
        return;
    const auto quantity = quantity_of(layer);
    const nodes::SandboxScale scale = nodes::sandbox_scale(quantity);
    const auto value_at = [&](float f) {
        return scale.logarithmic ? std::pow(10.0f, std::log10(scale.min) + f * (std::log10(scale.max) - std::log10(scale.min)))
                                 : std::max(scale.min + f * (scale.max - scale.min), 1e-6f);
    };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = 220.0f * ui::UI_SCALE;
    const float height = ImGui::GetTextLineHeight();
    constexpr int STEPS = 32;
    for (int i = 0; i < STEPS; i++) {
        const ImU32 a = to_imgui(nodes::sandbox_colour(quantity, value_at(float(i) / STEPS)));
        const ImU32 b = to_imgui(nodes::sandbox_colour(quantity, value_at(float(i + 1) / STEPS)));
        draw_list->AddRectFilledMultiColor(
            ImVec2(origin.x + width * float(i) / STEPS, origin.y), ImVec2(origin.x + width * float(i + 1) / STEPS, origin.y + height), a, b, b, a);
    }
    ImGui::Dummy(ImVec2(width, height));
    ImGui::SameLine();
    ImGui::TextDisabled("%s %g - %g %s%s", scale.label, double(scale.min), double(scale.max), scale.unit, scale.logarithmic ? " (log)" : "");
}

void SandboxView::draw(nodes::AvaFrameSandboxNode& sandbox, nodes::MpmSolverNode& solver)
{
    const nodes::SandboxTerrain* terrain = sandbox.terrain();
    // Keep the map overlay and the peak layers current, also with the window closed.
    if (terrain != nullptr && ((open && is_mpm_peak(m_layer)) || terrain->sandbox_to_world))
        request_peaks(sandbox, solver);
    if (!open)
        return;
    const auto* preview = sandbox.preview_texture();

    const ImVec2 viewport = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowSize(ImVec2(620 * ui::UI_SCALE, 680 * ui::UI_SCALE), ImGuiCond_FirstUseEver);
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
    ImGui::TextDisabled("t = %.1f s, reference %s", double(solver.simulated_time()), terrain->reference_name.empty() ? "none" : terrain->reference_name.c_str());

    const bool has_reference = !terrain->reference_pft.empty();
    const char* layer_names[] = { "Particles (live)", "MPM peak thickness", "MPM peak velocity", "MPM peak pressure", "com1DFA peak thickness",
        "com1DFA peak velocity", "com1DFA peak pressure" };
    int layer = int(m_layer);
    if (!has_reference && is_reference(m_layer))
        layer = 0;
    ImGui::SetNextItemWidth(220 * ui::UI_SCALE);
    if (ImGui::Combo("Layer", &layer, layer_names, has_reference ? 7 : 4))
        layer_changed = true;
    if (Layer(layer) != m_layer) {
        m_layer = Layer(layer);
        m_layer_dirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Zoom to domain", &m_zoom_to_domain);
    draw_legend(m_layer);
    ImGui::TextDisabled("cyan release, green com1DFA footprint, white box simulation domain; faint: pft < %.1f m",
        double(nodes::SANDBOX_FOOTPRINT_THICKNESS));

    // Layer texture: recolour when the layer or the data changed.
    const std::string key = terrain->dataset_path + "/" + terrain->scenario;
    if (m_layer != Layer::Particles) {
        const std::string layer_key
            = key + "#" + std::to_string(int(m_layer)) + "#" + (is_mpm_peak(m_layer) ? std::to_string(m_peaks_time) : std::string("ref"));
        if (m_layer_dirty || layer_key != m_layer_key) {
            const auto quantity = quantity_of(m_layer);
            if (is_reference(m_layer) && has_reference) {
                sandbox.set_layer_image(nodes::colour_sandbox_raster(*terrain, reference_of(*terrain, quantity), terrain->reference_pft, quantity));
                m_layer_key = layer_key;
            } else if (is_mpm_peak(m_layer) && m_peaks && m_peaks_key == key) {
                sandbox.set_layer_image(nodes::colour_sandbox_raster(*terrain, values_of(*m_peaks, quantity), m_peaks->pft, quantity));
                m_layer_key = layer_key;
            } else {
                m_layer_key.clear(); // nothing to show yet (peaks still on their way)
            }
            m_layer_dirty = false;
        }
    }

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
    ImGui::InvisibleButton("##sandbox_canvas", ImVec2(std::max(canvas_max.x - origin.x, 1.0f), std::max(canvas_max.y - origin.y, 1.0f)));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(origin, canvas_max, true);
    // DEM (row 0 = north = top of the image), and the peak layer on the same grid.
    const ImVec2 dem_top_left = to_screen(glm::dvec2(0.0, terrain->size_metres.y));
    const ImVec2 dem_bottom_right = to_screen(glm::dvec2(terrain->size_metres.x, 0.0));
    draw_list->AddImage((ImTextureID)preview->texture_view().handle(), dem_top_left, dem_bottom_right);
    if (m_layer != Layer::Particles && sandbox.layer_texture() != nullptr && !m_layer_key.empty())
        draw_list->AddImage((ImTextureID)sandbox.layer_texture()->texture_view().handle(), dem_top_left, dem_bottom_right);

    // Particle raster over the domain (also row 0 = north).
    if (has_domain) {
        const ImVec2 top_left = to_screen(glm::dvec2(domain.min.x, domain.max.y));
        const ImVec2 bottom_right = to_screen(glm::dvec2(domain.max.x, domain.min.y));
        const auto output = solver.output_socket("texture").get_data();
        const auto* texture = std::get<nodes::data_type<const webgpu::raii::TextureWithSampler*>()>(output);
        if (m_layer == Layer::Particles && texture != nullptr)
            draw_list->AddImage((ImTextureID)texture->texture_view().handle(), top_left, bottom_right);
        draw_list->AddRect(top_left, bottom_right, IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
    }
    draw_list->PopClipRect();

    // Hover readout: position, terrain, and MPM against com1DFA at the cell.
    if (ImGui::IsItemHovered()) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const glm::dvec2 metres(view_min.x + double(mouse.x - origin.x) / scale, view_max.y - double(mouse.y - origin.y) / scale);
        const glm::ivec2 cell(int(metres.x / terrain->cellsize), int(terrain->grid_size.y) - 1 - int(metres.y / terrain->cellsize));
        if (cell.x >= 0 && cell.y >= 0 && cell.x < int(terrain->grid_size.x) && cell.y < int(terrain->grid_size.y)) {
            const glm::uvec2 c(cell);
            const size_t i = size_t(c.y) * terrain->grid_size.x + c.x;
            char line[160];
            std::snprintf(line, sizeof(line), "%.0f, %.0f m, height %.0f m%s", metres.x, metres.y, double(terrain->heights.pixel(c)),
                terrain->nodata.pixel(c) ? " (NODATA, filled)" : "");
            std::string text = line;
            if (terrain->release_depth.pixel(c) > 0.0f) {
                std::snprintf(line, sizeof(line), "\nrelease depth %.2f m (vertical)", double(terrain->release_depth.pixel(c)));
                text += line;
            }
            const bool have_mpm = m_peaks && m_peaks_key == key;
            text += "\n           MPM   com1DFA";
            const auto row = [&](const char* name, const std::vector<float>* mpm, const std::vector<float>& ref, const char* unit) {
                std::snprintf(line, sizeof(line), "\n%s %8.2f  %8.2f %s", name, mpm ? double((*mpm)[i]) : 0.0, ref.empty() ? 0.0 : double(ref[i]), unit);
                text += line;
            };
            row("pft", have_mpm ? &m_peaks->pft : nullptr, terrain->reference_pft, "m");
            row("pfv", have_mpm ? &m_peaks->pfv : nullptr, terrain->reference_pfv, "m/s");
            row("ppr", have_mpm ? &m_peaks->ppr : nullptr, terrain->reference_ppr, "kPa");
            ImGui::SetTooltip("%s", text.c_str());
        }
    }

    ImGui::End();
}

} // namespace webgpu_app
