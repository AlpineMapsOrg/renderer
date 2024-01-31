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

#if !(defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 15000))
#include <concepts>
#endif

#include <QByteArray>

#include "constants.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/srs.h"
#include "radix/TileHeights.h"
#include "radix/geometry.h"

namespace nucleus::tile_scheduler {

using TileId2DataMap = std::unordered_map<tile::Id, std::shared_ptr<QByteArray>, tile::Id::Hasher>;

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

namespace utils {
#if defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 15000)
    // #define STRING2(x) #x
    // #define STRING(x) STRING2(x)
    // #pragma message STRING(_LIBCPP_VERSION)

    // the android stl used by qt doesn't know concepts (it's at version 11.000 at the time of writing)
    template <class T, class U>
    concept convertible_to = std::is_convertible_v<T, U>;

    namespace detail {
        template< class T, class U >
        concept SameHelper = std::is_same_v<T, U>;
    }

    template< class T, class U >
    concept same_as = detail::SameHelper<T, U> && detail::SameHelper<U, T>;

#else
    template <class T, class U>
    concept convertible_to = std::convertible_to<T, U>;

    template <class T, class U>
    concept same_as = std::same_as<T, U>;
#endif

    inline tile::SrsAndHeightBounds make_bounds(const tile::Id& id, float min_height, float max_height)
    {
        const auto comp_scaled_alt = [](float world_y, float altitude) {
            // unoptimised version:
            //            const auto lat = srs::world_to_lat_long({ 0.0, world_y }).x;
            //            return srs::lat_long_alt_to_world({ lat, 0.0, altitude }).z;

            // optimised version:
            constexpr double pi = 3.1415926535897932384626433;
            constexpr unsigned int cSemiMajorAxis = 6378137;
            constexpr double cEarthCircumference = 2 * pi * cSemiMajorAxis;
            constexpr double cOriginShift = cEarthCircumference / 2.0;

            const float mercN = world_y * float(pi / cOriginShift);
            const float lat_rad = 2.0f * (std::atan(std::exp(mercN)) - float(pi / 4.0));
            return altitude / std::abs(std::cos(lat_rad));
        };

        const auto srs_bounds = srs::tile_bounds(id);
        const auto max_world_y = [&srs_bounds]() {
            return float(std::max(srs_bounds.max.y, -srs_bounds.min.y));
        }();
        //        const auto min_world_y = [&srs_bounds, &id]() {
        //            if (id.zoom_level == 0)
        //                return 0.;
        //            return std::min(std::abs(srs_bounds.min.y), std::abs(srs_bounds.max.y));    // max can have a smaller abs value in the southern hemisphere
        //        }();

        const auto max_altitude = comp_scaled_alt(max_world_y, max_height) + 0.5f; // +0.5 to account for float inaccuracy
        const auto min_altitude = min_height - 0.5f; // we are allowed to be conservative with the AABBs! old code: comp_scaled_alt(min_world_y, min_height);
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
        inline tile::SrsAndHeightBounds aabb(const tile::Id& id) const
        {
            const auto heights = tile_heights.query({ id.zoom_level, id.coords });
            return make_bounds(id, heights.first, heights.second);
        }
        static inline AabbDecoratorPtr make(TileHeights heights)
        {
            return std::make_shared<AabbDecorator>(std::move(heights));
        }
    };

    inline auto camera_frustum_contains_tile_old(const nucleus::camera::Frustum& frustum, const tile::SrsAndHeightBounds& aabb)
    {
        for (const auto& p : frustum.corners)
            if (aabb.contains(p))
                return true;
        const auto triangles = geometry::clip(geometry::triangulise(aabb), frustum.clipping_planes);
        return !triangles.empty();
    }

    inline auto camera_frustum_contains_tile(const nucleus::camera::Frustum& frustum, const tile::SrsAndHeightBounds& aabb)
    {
        // loosely based on https://bruop.github.io/improved_frustum_culling/
        struct Range {
            double min;
            double max;
        };

        const auto aabb_corner_in_direction = [&aabb](const glm::dvec3& direction) {
            glm::dvec3 p = aabb.min;
            if (direction.x > 0)
                p.x = aabb.max.x;
            if (direction.y > 0)
                p.y = aabb.max.y;
            if (direction.z > 0)
                p.z = aabb.max.z;
            return p;
        };
        const auto ranges_overlap = [](const Range& a, const Range& b) {
            return a.min <= b.max && b.min <= a.max;
        };

        bool all_inside = true;
        for (const auto& p : frustum.clipping_planes) {
            if (distance(p, aabb_corner_in_direction(p.normal)) <= 0)
                return false;
            all_inside = all_inside && (distance(p, aabb_corner_in_direction(-p.normal)) > 0);
        }

        if (all_inside)
            return true;

        for (const auto& p : frustum.corners)
            if (aabb.contains(p))
                return true;

        const auto position_along_direction = [](const glm::dvec3& position, const glm::dvec3& direction) { return glm::dot(position, direction); };

        const auto aabb_range_along_direction = [&](const glm::dvec3& direction) {
            const auto a = position_along_direction(aabb_corner_in_direction(direction), direction);
            const auto b = position_along_direction(aabb_corner_in_direction(-direction), direction);
            return Range { std::min(a, b), std::max(a, b) };
        };

        const auto frustum_range_along_direction = [&](const glm::dvec3& direction) {
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::lowest();
            for (const auto& c : frustum.corners) {
                const auto p = position_along_direction(c, direction);
                if (p < min)
                    min = p;
                if (p > max)
                    max = p;
            }
            return Range { min, max };
        };

        const auto frustum_and_aabb_ranges_overlap_along_direction = [&](const glm::dvec3& direction) {
            const auto aabb_range = aabb_range_along_direction(direction);
            const auto frustum_range = frustum_range_along_direction(direction);
            return ranges_overlap(aabb_range, frustum_range);
        };

        const auto frustum_edges = std::array {
            glm::normalize(frustum.corners[4] - frustum.corners[0]),
            glm::normalize(frustum.corners[5] - frustum.corners[1]),
            glm::normalize(frustum.corners[6] - frustum.corners[2]),
            glm::normalize(frustum.corners[7] - frustum.corners[3]),
            glm::normalize(frustum.corners[1] - frustum.corners[0]),
            glm::normalize(frustum.corners[3] - frustum.corners[0])
        };

        constexpr auto aabb_edges = std::array {
            glm::dvec3 { 1., 0., 0. },
            glm::dvec3 { 0., 1., 0. },
            glm::dvec3 { 0., 0., 1. }
        };

        for (const auto& direction : aabb_edges) {
            if (!frustum_and_aabb_ranges_overlap_along_direction(direction))
                return false;
        }

        for (const auto& fe : frustum_edges) {
            for (const auto& ae : aabb_edges) {
                const glm::dvec3 direction = glm::cross(fe, ae);
                if (std::abs(direction.x) < geometry::epsilon<double>
                    && std::abs(direction.y) < geometry::epsilon<double>
                    && std::abs(direction.z) < geometry::epsilon<double>)
                    continue; // parallel
                if (!frustum_and_aabb_ranges_overlap_along_direction(direction))
                    return false;
            }
        }
        return true;
    }

    inline auto refine_functor_float(const nucleus::camera::Definition &camera,
                                     const AabbDecoratorPtr &aabb_decorator,
                                     float error_threshold_px,
                                     float tile_size = 256)
    {
        constexpr auto sqrt2 = 1.414213562373095f;
        const auto camera_frustum = camera.frustum();
        auto refine =
            [camera_frustum, camera, error_threshold_px, tile_size, aabb_decorator](const tile::Id& tile) {
                if (tile.zoom_level >= 18)
                    return false;

                auto aabb = aabb_decorator->aabb(tile);
                if (!tile_scheduler::utils::camera_frustum_contains_tile(camera_frustum, aabb))
                    return false;
                const auto aabb_float = geometry::Aabb<3, float>{aabb.min - camera.position(),
                                                                 aabb.max - camera.position()};

                const auto distance = geometry::distance(aabb_float, glm::vec3{0, 0, 0});
                const auto pixel_size = sqrt2 * aabb_float.size().x / tile_size;

                return camera.to_screen_space(pixel_size, distance) >= error_threshold_px;
            };
        return refine;
    }

    inline auto refineFunctor(const nucleus::camera::Definition& camera,
        const AabbDecoratorPtr& aabb_decorator,
        float error_threshold_px,
        double tile_size = 256)
    {
        constexpr auto sqrt2 = 1.414213562373095;
        const auto camera_frustum = camera.frustum();
        auto refine = [&camera, camera_frustum, error_threshold_px, tile_size, aabb_decorator](const tile::Id& tile) {
            if (tile.zoom_level >= 18)
                return false;

            const auto aabb = aabb_decorator->aabb(tile);
            if (!tile_scheduler::utils::camera_frustum_contains_tile(camera_frustum, aabb))
                return false;

            const auto distance = float(geometry::distance(aabb, camera.position()));
            const auto pixel_size = float(sqrt2 * aabb.size().x / tile_size);

            return camera.to_screen_space(pixel_size, distance) >= error_threshold_px;
        };
        return refine;
    }

    inline uint64_t time_since_epoch()
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
}
}
