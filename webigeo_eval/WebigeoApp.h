/*****************************************************************************
 * weBIGeo
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

#pragma once

#include <QObject>
#include <memory>

#include <webgpu_engine/Context.h>
#include <webgpu_engine/PipelineManager.h>
#include <webgpu_engine/compute/nodes/NodeGraph.h>

#include "Settings.h"

namespace webigeo_eval {

class WebigeoApp : public QObject {
    Q_OBJECT

public:
    WebigeoApp();
    ~WebigeoApp();

    /**
     * Updates node settings in node graph. Is written to `output-dir/settings.json` after run finished.
     * @param node_graph_settings settings struct, typically loaded from settings json file
     */
    void update_settings(const Settings& node_graph_settings);

    /**
     * Executes the node graph and blocks until the execution has completed or failed.
     */
    void run();

public slots:
    void run_completed();
    void run_failed(webgpu_engine::compute::nodes::GraphRunFailureInfo info);

signals:
    void run_ended();

private:
    WGPUInstance m_webgpu_instance;
    WGPUAdapter m_webgpu_adapter;
    WGPUDevice m_device;
    std::unique_ptr<webgpu_engine::Context> m_context;
    std::unique_ptr<webgpu_engine::compute::nodes::NodeGraph> m_node_graph;
    Settings m_settings = {};
    bool m_run_ended = false;
};

} // namespace webigeo_eval
