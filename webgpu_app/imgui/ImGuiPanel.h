/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#pragma once

#include <QObject>

namespace webgpu_app {

class ImGuiPanel : public QObject {
    Q_OBJECT
public:
    virtual ~ImGuiPanel() = default;

    // Called once after initialization
    virtual void ready() { }

    // Called outside the main sidebar window
    virtual void draw() { }

    // Called inside the main sidebar ImGui::Begin/End block
    virtual void draw_panel() { }
};

} // namespace webgpu_app
