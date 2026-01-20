/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Markus Rampp
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

#include "Settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace webigeo_eval {

void Settings::write_to_json_file(const Settings& settings, const std::filesystem::path& output_path)
{
    QJsonObject object;

    // paths
    object["aabb_file_path"] = QString::fromStdString(settings.aabb_file_path);
    object["heightmap_texture_path"] = QString::fromStdString(settings.heightmap_texture_path);
    object["release_cells_texture_path"] = QString::fromStdString(settings.release_cells_texture_path);
    object["output_dir_path"] = QString::fromStdString(settings.output_dir_path);

    // hyper parameters
    object["resolution_multiplier"] = qint64(settings.resolution_multiplier);
    object["num_simulation_runs"] = qint64(settings.num_simulation_runs);
    object["num_particles_per_release_cell"] = qint64(settings.num_particles_per_release_cell);
    object["num_simulation_steps"] = qint64(settings.num_simulation_steps);
    object["simulation_step_length"] = settings.simulation_step_length;
    object["random_seed"] = qint64(settings.random_seed);

    // model parameters
    object["max_random_deviation"] = settings.max_random_deviation;
    object["persistence"] = settings.persistence;
    object["max_runout_angle"] = settings.max_runout_angle;

    // other model parameters (experimental)
    object["model_type"] = qint64(settings.model_type);
    object["friction_model"] = qint64(settings.friction_model_type);
    object["friction_coeff"] = settings.friction_coeff;
    object["drag_coeff"] = settings.drag_coeff;
    object["slab_thickness"] = settings.slab_thickness;
    object["density"] = settings.density;

    QJsonDocument doc;
    doc.setObject(object);

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly)) {
        qFatal() << "error: Failed to open settings file " << output_path.string() << " for writing: " << output_file.errorString();
    }
    output_file.write(doc.toJson());
    output_file.close();
}

Settings Settings::read_from_json_file(const std::filesystem::path& input_path)
{
    if (std::filesystem::is_directory(input_path)) {
        qFatal() << "error: input path" << input_path.string() << "is a directory";
    }

    QJsonObject object;
    {
        // Read input file
        QFile input_file(input_path);
        if (!input_file.open(QIODevice::ReadOnly)) {
            qFatal() << "error: Failed to open settings file " << input_path.string() << " for reading: " << input_file.errorString();
        }
        QByteArray data = input_file.readAll();
        input_file.close();

        // Parse input file as JSON
        QJsonDocument document;
        QJsonParseError json_parse_error;
        document = QJsonDocument::fromJson(data, &json_parse_error);
        if (document.isNull()) {
            qFatal() << "error: JSON parsing failed at offset " << json_parse_error.offset << ", " << json_parse_error.errorString();
        }
        assert(document.isObject());
        object = document.object();
    }

    Settings settings;

    // paths
    settings.aabb_file_path = object["aabb_file_path"].toString().toStdString();
    settings.release_cells_texture_path = object["release_cells_texture_path"].toString().toStdString();
    settings.heightmap_texture_path = object["heightmap_texture_path"].toString().toStdString();
    settings.output_dir_path = object["output_dir_path"].toString().toStdString();

    // hyper parameters
    if (object.contains("resolution_multiplier")) {
        settings.resolution_multiplier = uint32_t(object["resolution_multiplier"].toInteger());
    }
    if (object.contains("num_simulation_runs")) {
        settings.num_simulation_runs = uint32_t(object["num_simulation_runs"].toInteger());
    }
    if (object.contains("num_particles_per_release_cell")) {
        settings.num_particles_per_release_cell = uint32_t(object["num_particles_per_release_cell"].toInteger());
    }
    if (object.contains("num_simulation_steps")) {
        settings.num_simulation_steps = uint32_t(object["num_simulation_steps"].toInteger());
    }
    if (object.contains("simulation_step_length")) {
        settings.simulation_step_length = float(object["simulation_step_length"].toDouble());
    }
    if (object.contains("random_seed")) {
        settings.random_seed = uint32_t(object["random_seed"].toInteger());
    }

    // model parameters
    if (object.contains("max_random_deviation")) {
        settings.max_random_deviation = float(object["max_random_deviation"].toDouble());
    }
    if (object.contains("persistence")) {
        settings.persistence = float(object["persistence"].toDouble());
    }
    if (object.contains("max_runout_angle")) {
        settings.max_runout_angle = float(object["max_runout_angle"].toDouble());
    }

    // other model parameters (experimental)
    if (object.contains("model_type")) {
        settings.model_type = int(object["model_type"].toInteger());
    }
    if (object.contains("friction_model")) {
        settings.friction_model_type = int(object["friction_model"].toInteger());
    }
    if (object.contains("friction_coeff")) {
        settings.friction_coeff = float(object["friction_coeff"].toDouble());
    }
    if (object.contains("drag_coeff")) {
        settings.drag_coeff = float(object["drag_coeff"].toDouble());
    }
    if (object.contains("slab_thickness")) {
        settings.slab_thickness = float(object["slab_thickness"].toDouble());
    }
    if (object.contains("density")) {
        settings.density = float(object["density"].toDouble());
    }

    return settings;
}
} // namespace webigeo_eval
