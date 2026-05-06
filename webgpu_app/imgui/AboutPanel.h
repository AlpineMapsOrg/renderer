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

#pragma once

#include "ImGuiPanel.h"

namespace webgpu_app {

// Renders the copyright box with the About button, the About popup modal,
// and the disclaimer popup modal. All three are co-located here since the
// button and modals are tightly coupled.
class AboutPanel : public ImGuiPanel {
public:
    AboutPanel() = default;

    void on_first_frame() override;
    void draw() override;

private:
    bool m_show_about_popup = false;
    bool m_open_disclaimer = false;

    void draw_copyright_box();
    void draw_about_popup();
    void draw_disclaimer_popup();
};

} // namespace webgpu_app
