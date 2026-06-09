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

#include "TextureOverlayImGuiRenderer.h"

#ifdef __EMSCRIPTEN__
#include <webgpu_app/WebInterop.h>
#endif

#include <ImGuiFileDialog.h>
#include <QDebug>
#include <filesystem>
#include <imgui.h>

#include <nucleus/utils/geopng_decoder.h>
#include <nucleus/utils/image_loader.h>

namespace webgpu_app {

static int s_instance_counter = 0;

TextureOverlayImGuiRenderer::TextureOverlayImGuiRenderer(webgpu_engine::TextureOverlay& overlay)
    : QObject(nullptr)
    , OverlayImGuiRenderer(overlay)
    , m_texture_overlay(&overlay)
{
    const int id = s_instance_counter++;
    m_png_tag = "textureoverlay_" + std::to_string(id) + "_png";
    m_aabb_tag = "textureoverlay_" + std::to_string(id) + "_aabb";

#ifdef __EMSCRIPTEN__
    connect(&WebInterop::instance(), &WebInterop::file_uploaded, this, &TextureOverlayImGuiRenderer::on_file_uploaded);
#endif
}

#ifdef __EMSCRIPTEN__
void TextureOverlayImGuiRenderer::on_file_uploaded(const std::string& filename, const std::string& tag)
{
    if (tag == m_png_tag)
        apply_image_file(filename);
    else if (tag == m_aabb_tag)
        apply_aabb_from_file(filename);
}
#endif

void TextureOverlayImGuiRenderer::apply_image_file(const std::string& path)
{
    const auto qpath = QString::fromStdString(path);

    if (const auto image = nucleus::utils::image_loader::rgba8(qpath)) {
        bool likely_encoded = false;
        m_texture_overlay->settings.float_decode_range = nucleus::utils::geopng::scan_encoded_float_range(*image, likely_encoded);
        if (likely_encoded) {
            m_texture_overlay->settings.mode = webgpu_engine::TextureOverlay::Mode::EncodedFloat;
            m_texture_overlay->settings.filter_mode = webgpu_engine::TextureOverlay::FilterMode::Nearest;
        } else {
            m_texture_overlay->settings.mode = webgpu_engine::TextureOverlay::Mode::AlphaBlend;
            m_texture_overlay->settings.filter_mode = webgpu_engine::TextureOverlay::FilterMode::Linear;
        }
    }

    m_texture_overlay->load_image(qpath);
    m_loaded_image_path = path;

#ifndef __EMSCRIPTEN__
    const auto fspath = std::filesystem::path(path);
    m_last_dialog_directory = fspath.parent_path().string();
    bool aabb_found = false;
    for (const auto& candidate : nucleus::utils::geopng::possible_aabb_paths(fspath)) {
        if (!std::filesystem::exists(candidate))
            continue;
        const auto result = nucleus::utils::geopng::load_aabb_from_file(candidate);
        if (result.has_value()) {
            m_texture_overlay->settings.aabb = result.value();
            aabb_found = true;
            break;
        }
    }
    if (!aabb_found)
        qWarning() << "No sidecar AABB file found for" << qpath;
#endif

    m_texture_overlay->update_gpu_settings();
    m_needs_redraw = true;
}

void TextureOverlayImGuiRenderer::apply_aabb_from_file(const std::string& path)
{
    const auto result = nucleus::utils::geopng::load_aabb_from_file(path);
    if (result.has_value()) {
        m_texture_overlay->settings.aabb = result.value();
        m_texture_overlay->update_gpu_settings();
    } else {
        qWarning() << "Failed to load AABB:" << QString::fromStdString(result.error());
    }
    m_needs_redraw = true;
}

bool TextureOverlayImGuiRenderer::render_custom_settings()
{
    auto& s = m_texture_overlay->settings;
    bool changed = false;
    const bool linked = m_texture_overlay->is_linked();

    if (!linked) {
        if (m_loaded_image_path.empty())
            ImGui::TextDisabled("No image loaded");
        else
            ImGui::TextUnformatted(std::filesystem::path(m_loaded_image_path).filename().string().c_str());

        const float full_w = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
#ifdef __EMSCRIPTEN__
        if (ImGui::Button("Load PNG...", ImVec2(full_w * 0.666f, 0)))
            WebInterop::instance().open_file_dialog(".png", m_png_tag);
#else
        if (ImGui::Button("Load PNG...", ImVec2(full_w * 0.666f, 0))) {
            IGFD::FileDialogConfig config;
            config.path = m_last_dialog_directory.empty() ? "." : m_last_dialog_directory;
            ImGuiFileDialog::Instance()->OpenDialog(m_png_tag.c_str(), "Choose Image", ".png,.*", config);
        }
        if (ImGuiFileDialog::Instance()->Display(m_png_tag.c_str())) {
            if (ImGuiFileDialog::Instance()->IsOk())
                apply_image_file(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
        }
#endif

        ImGui::SameLine();

#ifdef __EMSCRIPTEN__
        if (ImGui::Button("Load AABB...", ImVec2(-1, 0)))
            WebInterop::instance().open_file_dialog(".txt", m_aabb_tag);
#else
        if (ImGui::Button("Load AABB...", ImVec2(-1, 0))) {
            IGFD::FileDialogConfig config;
            config.path = m_last_dialog_directory.empty() ? "." : m_last_dialog_directory;
            ImGuiFileDialog::Instance()->OpenDialog(m_aabb_tag.c_str(), "Choose AABB File", ".txt,.*", config);
        }
        if (ImGuiFileDialog::Instance()->Display(m_aabb_tag.c_str())) {
            if (ImGuiFileDialog::Instance()->IsOk())
                apply_aabb_from_file(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
        }
#endif
    }

    ImGui::PushItemWidth(-1);
    if (ImGui::DragScalarN("##aabb", ImGuiDataType_Double, &s.aabb.min.x, 4, 0.0001f, nullptr, nullptr, "%.5f")) {
        m_texture_overlay->update_gpu_settings();
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::TextDisabled("AABB: min_x  min_y  max_x  max_y");

    ImGui::Separator();

    if (ImGui::SliderFloat("Opacity", &s.opacity, 0.0f, 1.0f)) {
        m_texture_overlay->update_gpu_settings();
        changed = true;
    }

    const char* mode_items[] = { "Alpha-Blend", "Encoded Float" };
    int mode_idx = (s.mode == webgpu_engine::TextureOverlay::Mode::EncodedFloat) ? 1 : 0;
    if (ImGui::Combo("Mode", &mode_idx, mode_items, 2)) {
        s.mode = (mode_idx == 1) ? webgpu_engine::TextureOverlay::Mode::EncodedFloat : webgpu_engine::TextureOverlay::Mode::AlphaBlend;
        m_texture_overlay->update_gpu_settings();
        changed = true;
    }

    if (s.mode == webgpu_engine::TextureOverlay::Mode::EncodedFloat) {
        if (ImGui::DragFloat2("Float Range", &s.float_decode_range.x, 1.0f, -10000.0f, 10000.0f, "%.1f")) {
            m_texture_overlay->update_gpu_settings();
            changed = true;
        }
    }

    // When a texture is linked the sampler settings below arent necessary and hidden
    if (!linked) {
        const char* filter_items[] = { "Nearest", "Linear" };
        int filter_idx = (s.filter_mode == webgpu_engine::TextureOverlay::FilterMode::Linear) ? 1 : 0;
        if (ImGui::Combo("Filter Mode", &filter_idx, filter_items, 2))
            s.filter_mode = (filter_idx == 1) ? webgpu_engine::TextureOverlay::FilterMode::Linear : webgpu_engine::TextureOverlay::FilterMode::Nearest;

        ImGui::Checkbox("Use Mipmaps", &s.use_mipmaps);
        ImGui::SameLine();
        ImGui::TextDisabled("(takes effect on next image load)");
    }

    changed |= m_needs_redraw;
    m_needs_redraw = false;

    return changed;
}

} // namespace webgpu_app
