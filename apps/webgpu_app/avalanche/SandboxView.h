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

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace webgpu_compute::nodes {
class AvaFrameSandboxNode;
class MpmSolverNode;
struct SandboxPeaks;
}

namespace webgpu_app {

/// Top-down 2D view of an AvaFrame sandbox run: the DEM as hillshade with the release area
/// and the com1DFA footprint outline (the sandbox node's preview texture), and on top either
/// the solver's live particle raster or a peak raster - the MPM's or com1DFA's - on a shared
/// colour scale. Hovering compares the two at the cell. An interactive version of the
/// evaluation plots of the earlier weBIGeo avalanche model (docs/Evaluation.md upstream).
///
/// The map cannot show sandbox runs: synthetic datasets have no location, and real ones
/// would land on weBIGeo's terrain rather than on AvaFrame's DEM. Real ones also go onto the
/// map as an overlay (AvaFrameSandboxNode::update_map_overlay), refreshed from here.
class SandboxView {
public:
    SandboxView();
    ~SandboxView();

    /// Draws the window. Call every frame while the sandbox graph is active.
    void draw(webgpu_compute::nodes::AvaFrameSandboxNode& sandbox, webgpu_compute::nodes::MpmSolverNode& solver);

    bool open = true;

    enum class Layer : int { Particles = 0, MpmThickness, MpmVelocity, MpmPressure, RefThickness, RefVelocity, RefPressure };
    Layer layer() const { return m_layer; }
    void set_layer(Layer layer)
    {
        m_layer = layer;
        m_layer_dirty = true;
    }
    bool layer_changed = false; // set when the user picks a layer, for the panel to store it

private:
    void request_peaks(webgpu_compute::nodes::AvaFrameSandboxNode& sandbox, webgpu_compute::nodes::MpmSolverNode& solver);
    void draw_legend(Layer layer);

    Layer m_layer = Layer::Particles;
    bool m_zoom_to_domain = false;

    /* MPM peaks on the DEM grid, refreshed about once a second (asynchronous readback). The
     * request carries a shared flag, so a readback that lands after this view is gone is dropped. */
    std::shared_ptr<bool> m_alive;
    std::shared_ptr<webgpu_compute::nodes::SandboxPeaks> m_peaks;
    std::string m_peaks_key; // dataset/scenario the peaks belong to
    bool m_peaks_pending = false;
    bool m_layer_dirty = true; // the layer texture needs redrawing
    float m_peaks_time = -1.0f; // simulated time of the peaks shown
    std::chrono::steady_clock::time_point m_last_request;
    std::string m_layer_key; // what the layer texture currently holds
};

} // namespace webgpu_app
