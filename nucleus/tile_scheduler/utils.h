/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <iostream>

#include "nucleus/camera/Definition.h"
#include "nucleus/srs.h"
#include "sherpa/TileHeights.h"
#include "sherpa/geometry.h"

namespace nucleus::tile_scheduler {

class AabbDecorator;
using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;

class AabbDecorator {
    TileHeights tile_heights;
public:
    inline AabbDecorator(TileHeights tile_heights)
        : tile_heights(std::move(tile_heights))
    {
    }
    inline tile::SrsAndHeightBounds aabb(const tile::Id& id)
    {
        const auto bounds = srs::tile_bounds(id);
        const auto heights = tile_heights.query({ id.zoom_level, id.coords });
        return { .min = { bounds.min, heights.first }, .max = { bounds.max, heights.second } };
    }
    static inline AabbDecoratorPtr make(TileHeights heights)
    {
        return std::make_shared<AabbDecorator>(std::move(heights));
    }
};

inline glm::dvec3 nearestVertex(const nucleus::camera::Definition& camera, const std::vector<geometry::Triangle<3, double>>& triangles)
{
    const auto camera_distance = [&camera](const auto& point) {
        const auto delta = point - camera.position();
        return glm::dot(delta, delta);
    };
    glm::dvec3 nearest_point = triangles.front().front();
    auto nearest_distance = camera_distance(nearest_point);
    for (const auto& triangle : triangles) {
        for (const auto& point : triangle) {
            const auto current_distance = camera_distance(point);
            if (current_distance < nearest_distance) {
                nearest_point = point;
                nearest_distance = current_distance;
            }
        }
    }
    return nearest_point;
}

inline auto cameraFrustumContainsTile(const nucleus::camera::Definition& camera, const tile::SrsAndHeightBounds& aabb)
{
    // this test should be based only on the four frustum planes (top, left, bottom, right), because
    // the near and far planes are adjusted based on the loaded AABBs, and that results in  a chicken egg problem.
    const auto triangles = geometry::clip(geometry::triangulise(aabb), camera.four_clipping_planes());
    if (triangles.empty())
        return false;
    return true;
}

inline auto refineFunctor(const nucleus::camera::Definition& camera, const AabbDecoratorPtr& aabb_decorator, double error_threshold_px, double tile_size = 256)
{
    std::cout << "camera.virtual_resolution_size().x: " << camera.virtual_resolution_size().x << std::endl;

    const auto refine = [&camera, error_threshold_px, tile_size, aabb_decorator](const tile::Id& tile) {
        if (tile.zoom_level >= 18)
            return false;

        const auto tile_aabb = aabb_decorator->aabb(tile);

        // this test should be based only on the four frustum planes (top, left, bottom, right), because
        // the near and far planes are adjusted based on the loaded AABBs, and that results in  a chicken egg problem.
        const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.four_clipping_planes());
        if (triangles.empty())
            return false;
        const auto nearest_point = glm::dvec4(nearestVertex(camera, triangles), 1);
        const auto aabb_width = tile_aabb.size().x;
        const auto other_point_axis = camera.x_axis();
        const auto other_point = nearest_point + glm::dvec4(other_point_axis * aabb_width / tile_size, 0);
        const auto vp_mat = camera.world_view_projection_matrix();

        auto nearest_screenspace = vp_mat * nearest_point;
        nearest_screenspace /= nearest_screenspace.w;
        auto other_screenspace = vp_mat * other_point;
        other_screenspace /= other_screenspace.w;
        const auto clip_space_difference = length(glm::dvec2(nearest_screenspace - other_screenspace));

        return clip_space_difference * 0.5 * camera.virtual_resolution_size().x >= error_threshold_px;
    };
    return refine;
}
}
