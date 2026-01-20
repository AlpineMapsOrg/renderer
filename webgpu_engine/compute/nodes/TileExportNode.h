/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "Node.h"
#include <map>

#define MAX_STITCHED_IMAGE_SIZE 8192

namespace webgpu_engine::compute::nodes {

class TileExportNode : public Node {
    Q_OBJECT

public:
    struct ExportSettings {

        // If true, the right and bottom 1px wide edge will be ignored when stitching and writing out
        bool remove_overlap = true;

        // If true, the tiles will be stitched together into one image per zoom level
        bool stitch_tiles = true;

        // For slippyMap tiles this has to be set to true as y starts from the bottom
        bool stitch_inverted_y = true;

        // If set to true the AABBs of the stitched tiles in the EPSG3857 CRS will be exported in
        // an extra text file
        bool stitch_export_aabb_text_files = true;

        // Directory to save the tiles to
        std::string output_directory = "tile_export";
    };

    TileExportNode(WGPUDevice device, const ExportSettings& settings);

    void set_settings(const ExportSettings& settings);
    const ExportSettings& get_settings() const;

public slots:
    void run_impl() override;

private:
    WGPUDevice m_device;
    ExportSettings m_settings;
    bool m_should_output_files;

    int m_exported_tile_count;
    int m_total_tile_count;

    glm::uvec2 m_tile_size;

    std::map<radix::tile::Id, std::shared_ptr<QByteArray>> m_tile_data;

    static void write_aabb_file(const QString& file_path, const radix::geometry::Aabb<2, double>& bounds);
    void impl_single_texture();
    void impl_texture_array();
    void readback_done();
};

} // namespace webgpu_engine::compute::nodes
