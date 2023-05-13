/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <QByteArray>

#include "constants.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/srs.h"
#include "sherpa/TileHeights.h"
#include "sherpa/geometry.h"

namespace nucleus::tile_scheduler {

using TileId2DataMap = std::unordered_map<tile::Id, std::shared_ptr<const QByteArray>, tile::Id::Hasher>;

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
    return !triangles.empty();
}

namespace utils {
#if defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 15000)
    // #define STRING2(x) #x
    // #define STRING(x) STRING2(x)
    // #pragma message STRING(_LIBCPP_VERSION)

    // the android stl used by qt doesn't know concepts (it's at version 11.000 at the time of writing)
    template <class T, class U>
    concept convertible_to = std::is_convertible_v<T, U>;

#else
    template <class T, class U>
    concept convertible_to = std::convertible_to<T, U>;
#endif

    inline tile::SrsAndHeightBounds make_bounds(const tile::Id& id, float min_height, float max_height)
    {
        const auto comp_scaled_alt = [](auto world_y, auto altitude) {
            const auto lat = srs::world_to_lat_long({ 0.0, world_y }).x;
            return srs::lat_long_alt_to_world({ lat, 0.0, altitude }).z;
        };

        const auto srs_bounds = srs::tile_bounds(id);
        const auto max_world_y = [&srs_bounds]() {
            return std::max(srs_bounds.max.y, -srs_bounds.min.y);
        }();
        const auto min_world_y = [&srs_bounds, &id]() {
            if (id.zoom_level == 0)
                return 0.;
            return std::min(std::abs(srs_bounds.min.y), std::abs(srs_bounds.max.y));    // max can have a smaller abs value in the southern hemisphere
        }();

        const auto max_altitude = comp_scaled_alt(max_world_y, max_height);
        const auto min_altitude = comp_scaled_alt(min_world_y, min_height);
        return { .min = { srs_bounds.min, min_altitude }, .max = { srs_bounds.max, max_altitude } };
    }

    class AabbDecorator;
    using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;
    class AabbDecorator {
        TileHeights tile_heights;

    public:
        explicit inline AabbDecorator(TileHeights tile_heights)
            : tile_heights(std::move(tile_heights))
        {
        }
        inline tile::SrsAndHeightBounds aabb(const tile::Id& id)
        {
            const auto heights = tile_heights.query({ id.zoom_level, id.coords });
            return make_bounds(id, heights.first, heights.second);
        }
        static inline AabbDecoratorPtr make(TileHeights heights)
        {
            return std::make_shared<AabbDecorator>(std::move(heights));
        }
    };

    inline auto refineFunctor(const nucleus::camera::Definition& camera, const AabbDecoratorPtr& aabb_decorator, double error_threshold_px, double tile_size = 256)
    {
        auto refine = [&camera, error_threshold_px, tile_size, aabb_decorator](const tile::Id& tile) {
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

            return clip_space_difference * 0.5 * camera.viewport_size().x >= error_threshold_px;
        };
        return refine;
    }

    static uint64_t time_since_epoch()
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void write_tile_id_2_data_map(const TileId2DataMap& map, const std::filesystem::path& path);
    TileId2DataMap read_tile_id_2_data_map(const std::filesystem::path& path);
}
}
