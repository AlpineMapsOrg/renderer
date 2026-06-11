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
#include <webgpu/raii/TextureWithSampler.h>
#include <webgpu_compute/nodes/Node.h>

namespace webgpu_engine {
class Context;
class TextureOverlay;
} // namespace webgpu_engine

namespace webgpu_compute::nodes {

// A custom node that forwards the graph result to a TextureOverlay managed by the OverlayRenderer.
// Unlike a base compute node it knows the rendering layer: the OverlayRenderer registers it with the NodeRegistry.
class OverlayRenderNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(OverlayRenderNode)

    struct OverlaySettings {
        // false: link the source texture directly (non-owning).
        // true: copy the source into the overlay's own texture (requires CopySrc on the source).
        bool copy = false;
    };

    explicit OverlayRenderNode(webgpu_engine::Context& context);
    OverlayRenderNode(webgpu_engine::Context& context, const OverlaySettings& settings);
    ~OverlayRenderNode() override;

    void set_settings(const OverlaySettings& settings) { m_settings = settings; }
    const OverlaySettings& get_settings() const { return m_settings; }

public slots:
    void run_impl() override;

private:
    webgpu_engine::Context* m_context;
    OverlaySettings m_settings;
    std::weak_ptr<webgpu_engine::TextureOverlay> m_result_overlay; // weak: the user may delete it in the gui
};

} // namespace webgpu_compute::nodes
