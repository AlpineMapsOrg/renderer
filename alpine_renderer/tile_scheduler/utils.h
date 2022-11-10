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

#include "alpine_renderer/Camera.h"
#include "alpine_renderer/srs.h"
#include "alpine_renderer/utils/geometry.h"

namespace tile_scheduler {
inline glm::dvec3 nearestVertex(const Camera& camera, const std::vector<geometry::Triangle<3, double>>& triangles)
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

inline auto cameraFrustumContainsTile(const Camera& camera, const tile::Id& tile)
{
    const auto tile_aabb = srs::aabb(tile, 100, 4000);
    const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.clippingPlanes());
    if (triangles.empty())
        return false;
    return true;
}

inline auto refineFunctor(const Camera& camera, double error_threshold_px, double tile_size = 256)
{
    const auto refine = [&camera, error_threshold_px, tile_size](const tile::Id& tile) {
        if (tile.zoom_level >= 18)
            return false;

        const auto tile_aabb = srs::aabb(tile, 100, 4000);
        const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.clippingPlanes());
        if (triangles.empty())
            return false;
        const auto nearest_point = glm::dvec4(nearestVertex(camera, triangles), 1);
        const auto aabb_width = tile_aabb.max.x - tile_aabb.min.x;
        const auto other_point_axis = camera.xAxis();
        const auto other_point = nearest_point + glm::dvec4(other_point_axis * aabb_width / tile_size, 0);
        const auto vp_mat = camera.worldViewProjectionMatrix();

        auto nearest_screenspace = vp_mat * nearest_point;
        nearest_screenspace /= nearest_screenspace.w;
        auto other_screenspace = vp_mat * other_point;
        other_screenspace /= other_screenspace.w;
        const auto clip_space_difference = length(glm::dvec2(nearest_screenspace - other_screenspace));

        return clip_space_difference * 0.5 * camera.viewportSize().x >= error_threshold_px;
    };
    return refine;
}

}
