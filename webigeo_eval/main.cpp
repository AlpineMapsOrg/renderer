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

#include <QCoreApplication>

#include "Settings.h"
#include "WebigeoApp.h"
#include "util/error_logging.h"

void resolve_relative_paths(webigeo_eval::Settings& settings, std::filesystem::path settings_path)
{
    const auto settings_dir = settings_path.parent_path();

    if (std::filesystem::path(settings.aabb_file_path).is_relative()) {
        settings.aabb_file_path = (settings_dir / settings.aabb_file_path).string();
    }
    if (std::filesystem::path(settings.heightmap_texture_path).is_relative()) {
        settings.heightmap_texture_path = (settings_dir / settings.heightmap_texture_path).string();
    }
    if (std::filesystem::path(settings.release_cells_texture_path).is_relative()) {
        settings.release_cells_texture_path = (settings_dir / settings.release_cells_texture_path).string();
    }
    if (std::filesystem::path(settings.output_dir_path).is_relative()) {
        settings.output_dir_path = (settings_dir / settings.output_dir_path).string();
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Init QCoreApplication is necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    // Set custom logging handler for Qt
    qInstallMessageHandler(qt_logging_callback);

    if (argc != 2) {
        qFatal() << "usage: webigeo-egal <settings-file-path>";
    }

    std::filesystem::path settings_path = argv[1];
    if (!std::filesystem::exists(settings_path)) {
        qFatal() << "error: input-dir-path " << settings_path.string() << " does not exist";
    }

    // load and parse input file
    webigeo_eval::Settings pipeline_settings = webigeo_eval::Settings::read_from_json_file(settings_path);
    resolve_relative_paths(pipeline_settings, settings_path);

    webigeo_eval::WebigeoApp webigeo_app;
    webigeo_app.update_settings(pipeline_settings);
    webigeo_app.run();

    // NOTE: Please be aware that for WEB-Deployment renderer.start() is non-blocking!!
    // So Code at this point will run after initialization
    return 0;
}
