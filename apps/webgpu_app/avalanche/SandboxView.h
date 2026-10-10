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

#pragma once

namespace webgpu_compute::nodes {
class AvaFrameSandboxNode;
class MpmSolverNode;
}

namespace webgpu_app {

/// Top-down 2D view of an AvaFrame sandbox run: the DEM as hillshade with the release area
/// and the com1DFA footprint outline (the sandbox node's preview texture), and the solver's
/// particle raster on top, inside the outline of the simulation domain.
///
/// The map cannot show sandbox runs: synthetic datasets have no location, and real ones
/// would land on weBIGeo's terrain rather than on AvaFrame's DEM.
class SandboxView {
public:
    /// Draws the window. Call every frame while the sandbox graph is active.
    void draw(const webgpu_compute::nodes::AvaFrameSandboxNode& sandbox, webgpu_compute::nodes::MpmSolverNode& solver);

    bool open = true;

private:
    bool m_zoom_to_domain = false;
    bool m_show_particles = true;
};

} // namespace webgpu_app
