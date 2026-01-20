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

#include "LoadRegionAabbNode.h"

#include <QFile>

namespace webgpu_engine::compute::nodes {

LoadRegionAabbNode::LoadRegionAabbNode()
    : LoadRegionAabbNode(LoadRegionAabbNodeSettings())
{
}

LoadRegionAabbNode::LoadRegionAabbNode(const LoadRegionAabbNodeSettings& settings)
    : Node({},
          {
              OutputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_output_bounds; }),
          })
    , m_settings(settings)
{
}

void LoadRegionAabbNode::set_settings(const LoadRegionAabbNodeSettings& settings) { m_settings = settings; }

void LoadRegionAabbNode::run_impl()
{
    qDebug() << "running LoadRegionAabbNode ...";
    qDebug() << "loading region aabb txt file from " << m_settings.file_path;

    tl::expected<radix::geometry::Aabb<2, double>, std::string> expected_region_aabb = load_aabb_from_file(m_settings.file_path);

    if (!expected_region_aabb.has_value()) {
        emit run_failed(NodeRunFailureInfo(*this, std::format("Failed to load aabb region file from {}: {}", m_settings.file_path, expected_region_aabb.error())));
        return;
    }

    m_output_bounds = expected_region_aabb.value();
    emit run_completed();
}

tl::expected<radix::geometry::Aabb<2, double>, std::string> LoadRegionAabbNode::load_aabb_from_file(const std::string& file_path) { return load_aabb_from_file(QString::fromStdString(file_path)); }

tl::expected<radix::geometry::Aabb<2, double>, std::string> LoadRegionAabbNode::load_aabb_from_file(const QString& file_path)
{
    QFile aabb_file(file_path);
    if (!aabb_file.open(QIODevice::ReadOnly)) {
        return tl::make_unexpected(std::format("Failed to open file {}", file_path.toStdString()));
    }
    QTextStream file_contents(&aabb_file);

    // parse extent file (very barebones rn, in the future we want to use geotiff anyway)
    // extent file contains the aabb of the aabb region (in world coordinates) the image overlay texture is associated with
    // each line contains exactly one floating point number (. as separator) with the following meaning:
    //   min_x
    //   min_y
    //   max_x
    //   max_y
    std::array<float, 4> contents;
    bool float_conversion_ok = false;
    for (size_t i = 0; i < contents.size(); i++) {
        QString line = file_contents.readLine();
        contents[i] = line.toFloat(&float_conversion_ok);
        if (!float_conversion_ok) {
            return tl::make_unexpected(std::format("Failed to parse file {}: Could not convert \"{}\" to float", file_path.toStdString(), line.toStdString()));
        }
    }

    if (contents[0] >= contents[2]) {
        return tl::make_unexpected(std::format("Failed to parse file {}: x_min ({}) must not be >= x_max ({})", file_path.toStdString(), contents[0], contents[2]));
    }

    if (contents[1] >= contents[3]) {
        return tl::make_unexpected(std::format("Failed to parse file {}: y_min ({}) must not be >= y_max ({})", file_path.toStdString(), contents[1], contents[3]));
    }

    return radix::geometry::Aabb<2, double> { { contents[0], contents[1] }, { contents[2], contents[3] } };
}

} // namespace webgpu_engine::compute::nodes
