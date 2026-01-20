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

#include "util.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace webgpu_engine::compute::nodes {

void write_timings_to_json_file(const NodeGraph& node_graph, const std::filesystem::path& output_path)
{
    QJsonObject object;
    for (const auto& [name, node] : node_graph.get_nodes()) {
        object[QString::fromStdString(name)] = node.get()->get_last_run_duration_in_ms();
    }

    QJsonDocument doc;
    doc.setObject(object);

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly))
        qFatal("Failed to open timings file for writing: %s", output_path.string().c_str());
    output_file.write(doc.toJson());
    output_file.close();
}
} // namespace webgpu_engine::compute::nodes
