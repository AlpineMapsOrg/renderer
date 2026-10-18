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

#include <memory>
#include <radix/geometry.h>
#include <webgpu/base/raii/TextureWithSampler.h>
#include <webgpu/compute/nodes/Node.h>

namespace webgpu_engine {
class Context;
class TextureOverlay;
} // namespace webgpu_engine

namespace webgpu_compute::nodes {

// A custom node that forwards the graph result to a TextureOverlay managed by the OverlayRenderer.
// Unlike a base compute node it knows the rendering layer.
class OverlayRenderNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(OverlayRenderNode)

    struct OverlaySettings {
        // false: link the source texture directly (non-owning).
        // true: copy the source into the overlay's own texture (requires CopySrc on the source).
        bool copy = false;
        // Trim the AABB by the column/row the tile stitch node leaves out. Off for inputs that
        // do not come from stitched tiles (the AvaFrame sandbox's map overlay).
        bool trim_stitch_edge = true;
    };

    explicit OverlayRenderNode(webgpu_engine::Context& context);
    OverlayRenderNode(webgpu_engine::Context& context, const OverlaySettings& settings);
    ~OverlayRenderNode() override;

    void set_settings(const OverlaySettings& settings) { m_settings = settings; }
    /// Shows or hides the overlay this node feeds (e.g. the 2D density raster while the 3D
    /// particles are drawn). No effect before its first run.
    void set_overlay_enabled(bool enabled);
    const OverlaySettings& get_settings() const { return m_settings; }
    void serialize_settings(QJsonObject& out) const override
    {
        out["copy"] = m_settings.copy;
        out["trim_stitch_edge"] = m_settings.trim_stitch_edge;
    }
    void deserialize_settings(const QJsonObject& in) override
    {
        if (in.contains("copy"))
            m_settings.copy = in["copy"].toBool(m_settings.copy);
        if (in.contains("trim_stitch_edge"))
            m_settings.trim_stitch_edge = in["trim_stitch_edge"].toBool(m_settings.trim_stitch_edge);
    }

public slots:
    void run_impl() override;

private:
    webgpu_engine::Context* m_context;
    OverlaySettings m_settings;
    std::weak_ptr<webgpu_engine::TextureOverlay> m_result_overlay; // weak: the user may delete it in the gui
};

} // namespace webgpu_compute::nodes
