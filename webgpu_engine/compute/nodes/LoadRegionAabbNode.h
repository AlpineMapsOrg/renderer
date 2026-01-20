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

#include "Node.h"

#include <tl/expected.hpp>

namespace webgpu_engine::compute::nodes {

class LoadRegionAabbNode : public Node {
    Q_OBJECT

public:
    struct LoadRegionAabbNodeSettings {
        // path to region aabb txt file to load
        std::string file_path;
    };

    LoadRegionAabbNode();
    LoadRegionAabbNode(const LoadRegionAabbNodeSettings& settings);

    void set_settings(const LoadRegionAabbNodeSettings& settings);

public slots:
    void run_impl() override;

public:
    // TODO move to some utility file; should also be used from Window.cpp (duplicate code)
    static tl::expected<radix::geometry::Aabb<2, double>, std::string> load_aabb_from_file(const std::string& file_path);
    static tl::expected<radix::geometry::Aabb<2, double>, std::string> load_aabb_from_file(const QString& file_path);

private:
    LoadRegionAabbNodeSettings m_settings;
    radix::geometry::Aabb<2, double> m_output_bounds;
};

} // namespace webgpu_engine::compute::nodes
