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

#pragma once

#include <QObject>
#include <imgui.h>

#include "ImGuiPanel.h"
#include "SearchService.h"

namespace webgpu_app {

class TerrainRenderer; // fwd decl

class SearchPanel : public ImGuiPanel {
    Q_OBJECT
public:
    explicit SearchPanel(TerrainRenderer* renderer);

    void draw() override;

public slots:
    void display_search_results(const std::vector<webgpu_app::SearchResult>& search_results);

signals:
    void search_requested(const std::string& searchText);
    void search_result_selected(double latitude, double longitude);

private:
    void select_result(const SearchResult& result);
    TerrainRenderer* m_terrain_renderer;
    std::vector<SearchResult> m_search_results;
    std::array<char, 128> m_search_buffer {};
    bool m_set_focus_on_text = true;
    bool m_is_active = false;
    bool m_show_no_results = false;
    bool m_clear_input_requested = false;
};

} // namespace webgpu_app
