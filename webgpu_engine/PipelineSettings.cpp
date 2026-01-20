/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "PipelineSettings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace webgpu_engine {

void ComputePipelineSettings::write_to_json_file(const ComputePipelineSettings& settings, const std::filesystem::path& output_path)
{
    QJsonObject object;
    object["tile_source"] = settings.tile_source_index == 0 ? "dtm" : "dsm";
    object["source_zoomlevel"] = qint64(settings.zoomlevel);

    object["release_point_interval"] = qint64(settings.release_point_interval);
    object["trigger_point_min_slope_angle"] = settings.trigger_point_min_slope_angle;
    object["trigger_point_max_slope_angle"] = settings.trigger_point_max_slope_angle;

    object["num_steps"] = qint64(settings.num_steps);
    object["num_paths_per_release_cell"] = qint64(settings.num_paths_per_release_cell);

    object["random_seed"] = qint64(settings.random_seed);
    object["random_contribution"] = settings.random_contribution;
    object["persistence_contribution"] = settings.persistence_contribution;
    object["alpha"] = settings.runout_flowpy_alpha;

    object["trajectory_resolution_multiplier"] = qint64(settings.trajectory_resolution_multiplier);

    QJsonDocument doc;
    doc.setObject(object);

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly))
        qFatal("Failed to open pipeline settings file for writing: %s", output_path.string().c_str());
    output_file.write(doc.toJson());
    output_file.close();
}

ComputePipelineSettings ComputePipelineSettings::read_from_json_file(const std::filesystem::path& input_path)
{
    QJsonObject object;
    {
        QJsonDocument document;

        QFile input_file(input_path);
        if (!input_file.open(QIODevice::ReadOnly))
            qFatal("Failed to open pipeline settings file for reading: %s", input_path.string().c_str());

        QByteArray data = input_file.readAll();
        document = QJsonDocument::fromJson(data);
        input_file.close();
        assert(document.isObject());
        object = document.object();
    }

    ComputePipelineSettings settings;
    settings.tile_source_index = object["tile_source"] == "dtm" ? 0 : 1;
    settings.zoomlevel = uint32_t(object["source_zoomlevel"].toInteger());

    settings.release_point_interval = uint32_t(object["release_point_interval"].toInteger());
    settings.trigger_point_min_slope_angle = float(object["trigger_point_min_slope_angle"].toDouble());
    settings.trigger_point_max_slope_angle = float(object["trigger_point_max_slope_angle"].toDouble());

    if (object.contains("random_seed")) {
        settings.random_seed = uint32_t(object["random_seed"].toInteger());
    }
    settings.num_steps = uint32_t(object["num_steps"].toInteger());
    settings.num_paths_per_release_cell = uint32_t(object["num_paths_per_release_cell"].toInteger());

    settings.random_contribution = float(object["random_contribution"].toDouble());
    settings.persistence_contribution = float(object["persistence_contribution"].toDouble());
    settings.runout_flowpy_alpha = float(object["alpha"].toDouble());

    settings.trajectory_resolution_multiplier = uint32_t(object["trajectory_resolution_multiplier"].toInteger());
    return settings;
}

} // namespace webgpu_engine
