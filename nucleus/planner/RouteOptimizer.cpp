/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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

#include "RouteOptimizer.h"

#ifndef __EMSCRIPTEN__
// the lite WebAssembly Qt is built without the future feature, so it has no QFutureWatcher / Qt Concurrent
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#endif
#include <QTimer>

#include <nucleus/srs.h>
#include <nucleus/tile/constants.h>
#include <nucleus/tile/TileLoadService.h>
#include <nucleus/tile/types.h>
#include <nucleus/utils/image_loader.h>
#include <radix/height_encoding.h>
#include <radix/tile.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>

namespace nucleus::planner {

namespace {
constexpr unsigned k_dem_tile_pixels = 65;      // dem tiles are 65x65 pixels
constexpr unsigned k_max_grid_dimension = 640;  // cells per raster axis
constexpr unsigned k_max_tiles = 256;           // per optimisation request
constexpr int k_max_zoom_level = 18;            // same cap as the geometry scheduler
constexpr int k_min_zoom_level = 12;
constexpr int k_fetch_timeout_ms = 30000;
constexpr int k_anchor_search_radius = 10;      // cells, to anchor waypoints to valid cells

// penalty metres per metre travelled for traversing the given slope angle.
// piecewise linear: 0 at the ideal 30-45 degrees, avoided below 20 degrees,
// increasingly expensive up to 60 degrees, rappel territory above.
double gradient_penalty_impl(double slope_degrees)
{
    if (slope_degrees < 20.0)
        return 4.0 - (slope_degrees / 20.0) * 3.0;
    if (slope_degrees < 30.0)
        return 1.0 - (slope_degrees - 20.0) / 10.0;
    if (slope_degrees <= 45.0)
        return 0.0;
    if (slope_degrees <= 60.0)
        return (slope_degrees - 45.0) / 15.0 * 2.5;
    if (slope_degrees <= 70.0)
        return 2.5 + (slope_degrees - 60.0) / 10.0 * 9.5;
    return 40.0;
}

// penalty metres per metre travelled for CLIMBING the given slope angle
// (skin-friendly terrain): the optimum band is 20-25 degrees, 15-20 degrees
// is fine, flatter is suboptimal, above 30 degrees the skins come off and
// every additional degree of bootpack steepness gets expensive fast
double uphill_penalty_impl(double slope_degrees)
{
    if (slope_degrees < 15.0)
        return 3.0 - (slope_degrees / 15.0) * 2.25;
    if (slope_degrees < 20.0)
        return 0.75 - (slope_degrees - 15.0) / 5.0 * 0.75;
    if (slope_degrees <= 25.0)
        return 0.0;
    if (slope_degrees <= 30.0)
        return (slope_degrees - 25.0) / 5.0 * 3.0;
    if (slope_degrees <= 40.0)
        return 3.0 + (slope_degrees - 30.0) / 10.0 * 9.0;
    return 12.0 + (slope_degrees - 40.0) * 0.8;
}

} // namespace

double gradient_penalty(double slope_degrees) { return gradient_penalty_impl(slope_degrees); }

double uphill_penalty(double slope_degrees) { return uphill_penalty_impl(slope_degrees); }

double cross_slope_penalty(double cross_slope_degrees)
{
    // quadratic rise to 4 penalty metres per metre at 40 degrees side-tilt,
    // saturating beyond. unlike the gradient penalty there is no "ideal"
    // side-tilt: any amount is at best neutral, steep side-tilt is bad
    const double t = std::clamp(cross_slope_degrees / 40.0, 0.0, 1.0);
    return 4.0 * t * t;
}

// ------------------------------------------------------------------------------------------------
// DemTile / DemRaster

std::optional<float> DemTile::sample(double world_x, double world_y) const
{
    if (world_x < min_x || world_x > max_x || world_y < min_y || world_y > max_y) {
        return std::nullopt;
    }
    const double u = (world_x - min_x) / (max_x - min_x);
    const double v = (world_y - min_y) / (max_y - min_y);
    // bilinear between the texel centres (row 0 = north), matching how the
    // rendered terrain interpolates the height texture
    const double fx = u * double(width) - 0.5;
    const double fy = (1.0 - v) * double(height) - 0.5;
    const double x0 = std::clamp(std::floor(fx), 0.0, double(width) - 1.0);
    const double y0 = std::clamp(std::floor(fy), 0.0, double(height) - 1.0);
    const double x1 = std::min(x0 + 1.0, double(width) - 1.0);
    const double y1 = std::min(y0 + 1.0, double(height) - 1.0);
    const double tx = std::clamp(fx - x0, 0.0, 1.0);
    const double ty = std::clamp(fy - y0, 0.0, 1.0);

    const auto texel = [&](double px, double py) {
        return heights[size_t(py) * width + size_t(px)];
    };
    const double top = texel(x0, y0) * (1.0 - tx) + texel(x1, y0) * tx;
    const double bottom = texel(x0, y1) * (1.0 - tx) + texel(x1, y1) * tx;
    return float(top * (1.0 - ty) + bottom * ty);
}

std::optional<float> DemTile::mesh_sample(double world_x, double world_y) const
{
    if (world_x < min_x || world_x > max_x || world_y < min_y || world_y > max_y) {
        return std::nullopt;
    }
    const double u = (world_x - min_x) / (max_x - min_x);
    const double v = (world_y - min_y) / (max_y - min_y);
    // the rendered mesh places a vertex per texel, spanning the tile with
    // (width-1) x (height-1) cells; row 0 is the north edge
    const double fx = u * double(width - 1);
    const double fy = (1.0 - v) * double(height - 1);
    const double cx = std::clamp(std::floor(fx), 0.0, double(width) - 2.0);
    const double cy = std::clamp(std::floor(fy), 0.0, double(height) - 2.0);
    const double tx = std::clamp(fx - cx, 0.0, 1.0);
    const double ty = std::clamp(fy - cy, 0.0, 1.0);

    const auto texel = [&](double px, double py) {
        return heights[size_t(py) * width + size_t(px)];
    };
    const double north_west = texel(cx, cy);
    const double north_east = texel(cx + 1, cy);
    const double south_west = texel(cx, cy + 1);
    const double south_east = texel(cx + 1, cy + 1);

    // triangle split along the south-west / north-east diagonal (u+v vs 1)
    if (tx + ty <= 1.0) {
        return float(north_west + (north_east - north_west) * tx + (south_west - north_west) * ty);
    }
    return float(south_east + (south_west - south_east) * (1.0 - tx) + (north_east - south_east) * (1.0 - ty));
}

std::optional<DemRaster> DemRaster::create(const std::vector<DemTile>& tiles,
                                           glm::dvec2 bbox_min, glm::dvec2 bbox_max,
                                           double cell_size)
{
    // don't coarsen the dem resolution away: z18 tiles resolve to ~2.4 m per
    // sample, and the rendered terrain uses the same tiles
    cell_size = std::clamp(cell_size, 3.0, 50.0);
    // grow the cell size if the area is large, so the grid stays bounded
    const double span_x = bbox_max.x - bbox_min.x;
    const double span_y = bbox_max.y - bbox_min.y;
    cell_size = std::max(cell_size, span_x / double(k_max_grid_dimension));
    cell_size = std::max(cell_size, span_y / double(k_max_grid_dimension));

    DemRaster raster;
    raster.m_min = bbox_min;
    raster.m_cell_size = cell_size;
    raster.m_cols = unsigned(span_x / cell_size) + 1;
    raster.m_rows = unsigned(span_y / cell_size) + 1;
    raster.m_altitudes.assign(size_t(raster.m_cols) * raster.m_rows, std::numeric_limits<float>::quiet_NaN());

    bool any_data = false;
    for (const DemTile& tile : tiles) {
        const unsigned col_begin = unsigned(std::max(0.0, (tile.min_x - bbox_min.x) / cell_size));
        const unsigned col_end = std::min(raster.m_cols, unsigned(std::ceil((tile.max_x - bbox_min.x) / cell_size)) + 1);
        const unsigned row_begin = unsigned(std::max(0.0, (tile.min_y - bbox_min.y) / cell_size));
        const unsigned row_end = std::min(raster.m_rows, unsigned(std::ceil((tile.max_y - bbox_min.y) / cell_size)) + 1);
        for (unsigned row = row_begin; row < row_end; ++row) {
            for (unsigned col = col_begin; col < col_end; ++col) {
                const auto [wx, wy] = raster.cell_center(col, row);
                const auto value = tile.sample(wx, wy);
                if (value) {
                    raster.m_altitudes[size_t(row) * raster.m_cols + col] = *value;
                    any_data = true;
                }
            }
        }
    }

    if (!any_data) {
        return std::nullopt;
    }

    // terrain slope per cell from central differences (one-sided at the
    // borders). this is the steepness of the dem itself, independent of the
    // direction of travel. the gradient vector is kept too, to split the
    // slope into along-travel and across-travel parts
    raster.m_gradients.assign(raster.m_altitudes.size(), std::numeric_limits<float>::quiet_NaN());
    raster.m_gradient_xy.assign(raster.m_altitudes.size(),
                                glm::vec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()));
    const auto altitude_at = [&raster](int col, int row) -> double {
        const auto value = raster.cell_altitude(unsigned(col), unsigned(row));
        return value ? double(*value) : std::numeric_limits<double>::quiet_NaN();
    };
    for (unsigned row = 0; row < raster.m_rows; ++row) {
        for (unsigned col = 0; col < raster.m_cols; ++col) {
            const double centre = altitude_at(int(col), int(row));
            if (std::isnan(centre)) {
                continue;
            }
            // pick usable x/y neighbour pairs; one-sided when a side is
            // missing (border or no-data), flat when both are missing
            const double west = altitude_at(col > 0 ? int(col) - 1 : int(col), int(row));
            const double east = altitude_at(col + 1 < raster.m_cols ? int(col) + 1 : int(col), int(row));
            const double south = altitude_at(int(col), row > 0 ? int(row) - 1 : int(row));
            const double north = altitude_at(int(col), row + 1 < raster.m_rows ? int(row) + 1 : int(row));

            const double dx = ((col + 1 < raster.m_cols ? int(col) + 1 : int(col)) - (col > 0 ? int(col) - 1 : int(col))) * raster.m_cell_size;
            const double dy = ((row + 1 < raster.m_rows ? int(row) + 1 : int(row)) - (row > 0 ? int(row) - 1 : int(row))) * raster.m_cell_size;

            const double dzdx = std::isnan(east) ? (std::isnan(west) ? 0.0 : (centre - west) / dx)
                                                 : (std::isnan(west) ? (east - centre) / dx
                                                                     : (east - west) / dx);
            const double dzdy = std::isnan(north) ? (std::isnan(south) ? 0.0 : (centre - south) / dy)
                                                  : (std::isnan(south) ? (north - centre) / dy
                                                                       : (north - south) / dy);
            raster.m_gradient_xy[size_t(row) * raster.m_cols + col] = glm::vec2(float(dzdx), float(dzdy));
            raster.m_gradients[size_t(row) * raster.m_cols + col]
                = float(glm::degrees(std::atan(std::hypot(dzdx, dzdy))));
        }
    }
    return raster;
}

glm::dvec2 DemRaster::cell_center(unsigned col, unsigned row) const
{
    return { m_min.x + (double(col) + 0.5) * m_cell_size, m_min.y + (double(row) + 0.5) * m_cell_size };
}

std::optional<float> DemRaster::cell_altitude(unsigned col, unsigned row) const
{
    if (col >= m_cols || row >= m_rows) {
        return std::nullopt;
    }
    const float value = m_altitudes[size_t(row) * m_cols + col];
    if (std::isnan(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<float> DemRaster::cell_gradient(unsigned col, unsigned row) const
{
    if (col >= m_cols || row >= m_rows) {
        return std::nullopt;
    }
    const float value = m_gradients[size_t(row) * m_cols + col];
    if (std::isnan(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<glm::dvec2> DemRaster::cell_gradient_vector(unsigned col, unsigned row) const
{
    if (col >= m_cols || row >= m_rows) {
        return std::nullopt;
    }
    const glm::vec2 value = m_gradient_xy[size_t(row) * m_cols + col];
    if (std::isnan(value.x) || std::isnan(value.y)) {
        return std::nullopt;
    }
    return glm::dvec2(value);
}

std::optional<float> DemRaster::altitude(glm::dvec2 world) const
{
    if (!inside(world)) {
        return std::nullopt;
    }
    // bilinear between the four surrounding cell centres
    const double fx = (world.x - m_min.x) / m_cell_size - 0.5;
    const double fy = (world.y - m_min.y) / m_cell_size - 0.5;
    const unsigned col0 = unsigned(std::max(0.0, std::floor(fx)));
    const unsigned row0 = unsigned(std::max(0.0, std::floor(fy)));
    const double tx = fx - double(col0);
    const double ty = fy - double(row0);

    const auto c00 = cell_altitude(col0, row0);
    const auto c10 = cell_altitude(col0 + 1, row0);
    const auto c01 = cell_altitude(col0, row0 + 1);
    const auto c11 = cell_altitude(col0 + 1, row0 + 1);

    // fall back to the nearest valid corner when a neighbour has no data
    std::optional<float> result;
    double best_weight = -1.0;
    const std::pair<std::optional<float>, double> corners[4] = {
        { c00, (1.0 - tx) * (1.0 - ty) }, { c10, tx * (1.0 - ty) },
        { c01, (1.0 - tx) * ty },         { c11, tx * ty },
    };
    for (const auto& [value, weight] : corners) {
        if (!value || weight <= best_weight) {
            continue;
        }
        result = value;
        best_weight = weight;
    }
    return result;
}

bool DemRaster::inside(glm::dvec2 world) const
{
    return world.x >= m_min.x && world.x <= m_min.x + double(m_cols) * m_cell_size
        && world.y >= m_min.y && world.y <= m_min.y + double(m_rows) * m_cell_size;
}

// ------------------------------------------------------------------------------------------------
// a*

std::optional<std::vector<glm::ivec2>> optimise_segment(const DemRaster& dem,
                                                        glm::ivec2 start, glm::ivec2 goal,
                                                        double steepness_weight, double distance_weight,
                                                        double cross_slope_weight,
                                                        double uphill_steepness_weight, double climb_penalty)
{
    // the cost mix is normalised per travel direction: descending edges use
    // the (downhill) steepness weight, climbing edges the uphill one, and the
    // distance weight is shared by both mixes
    const double down_steep = std::max(steepness_weight, 2.0);
    const double up_steep = std::max(uphill_steepness_weight, 2.0);
    // keep a small distance floor: with pure steepness weighting crossing
    // ideal terrain costs zero, which makes the search drift along zero-cost
    // plateaus instead of preferring the direct line
    const double distance = std::max(distance_weight, 0.0);
    const double down_sum = distance + down_steep;
    const double up_sum = distance + up_steep;
    const double down_dist_frac = distance / down_sum;
    const double down_steep_frac = down_steep / down_sum;
    const double up_dist_frac = distance / up_sum;
    const double up_steep_frac = up_steep / up_sum;
    // surcharge per vertical metre climbed (in penalty metres), scaled by the
    // uphill-penalty slider. climbing is avoided aggressively: at the default
    // settings a 100 m climb costs about as much as walking half a kilometre
    // on ideal-slope terrain, so the optimiser prefers detours around hills
    const double climb_surcharge = std::max(climb_penalty, 0.0);
    // 0..2 multiplier from the ui slider
    const double cross_weight = std::clamp(cross_slope_weight, 0.0, 2.0);

    const auto anchor = [&dem](glm::ivec2 cell) -> std::optional<glm::ivec2> {
        if (dem.cell_altitude(unsigned(cell.x), unsigned(cell.y))) {
            return cell;
        }
        for (int radius = 1; radius <= k_anchor_search_radius; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const glm::ivec2 candidate = cell + glm::ivec2(dx, dy);
                    if (dem.cell_altitude(unsigned(candidate.x), unsigned(candidate.y))) {
                        return candidate;
                    }
                }
            }
        }
        return std::nullopt;
    };

    const auto anchored_start = anchor(start);
    const auto anchored_goal = anchor(goal);
    if (!anchored_start || !anchored_goal) {
        return std::nullopt;
    }

    const auto goal_position = dem.cell_center(unsigned(anchored_goal->x), unsigned(anchored_goal->y));

    const auto heuristic = [&dem, &goal_position, down_dist_frac, up_dist_frac](glm::ivec2 cell) {
        const glm::dvec2 position = dem.cell_center(unsigned(cell.x), unsigned(cell.y));
        // admissible: every metre of horizontal progress costs at least the
        // smaller of the two direction mixes' distance fractions
        return std::min(down_dist_frac, up_dist_frac) * glm::length(goal_position - position);
    };

    struct QueueEntry {
        double priority;
        int cell;
        bool operator<(const QueueEntry& other) const { return priority > other.priority; }
    };

    const int cols = int(dem.cols());
    const int rows = int(dem.rows());
    const auto to_index = [cols](glm::ivec2 cell) { return cell.y * cols + cell.x; };

    std::priority_queue<QueueEntry> open_set;
    std::vector<double> path_cost(size_t(cols) * rows, std::numeric_limits<double>::infinity());
    std::vector<int> came_from(size_t(cols) * rows, -1);
    std::vector<uint8_t> closed(size_t(cols) * rows, 0);

    path_cost[size_t(to_index(*anchored_start))] = 0.0;
    open_set.push({ heuristic(*anchored_start), to_index(*anchored_start) });

    constexpr std::array<glm::ivec2, 8> neighbours = {
        glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1),
        glm::ivec2(1, 1), glm::ivec2(1, -1), glm::ivec2(-1, 1), glm::ivec2(-1, -1),
    };
    double goal_cost = std::numeric_limits<double>::infinity();
    int goal_index = -1;

    while (!open_set.empty()) {
        const auto [priority, index] = open_set.top();
        open_set.pop();
        if (closed[size_t(index)]) {
            continue;
        }
        closed[size_t(index)] = 1;

        const glm::ivec2 cell(index % cols, index / cols);
        if (cell == *anchored_goal) {
            goal_cost = path_cost[size_t(index)];
            goal_index = index;
            break;
        }
        if (priority > goal_cost) {
            break; // nothing cheaper can still be found
        }

        const auto from_altitude = dem.cell_altitude(unsigned(cell.x), unsigned(cell.y));
        const auto from_gradient_vec = dem.cell_gradient_vector(unsigned(cell.x), unsigned(cell.y));
        if (!from_altitude || !from_gradient_vec) {
            continue;
        }
        const glm::dvec2 from_position = dem.cell_center(unsigned(cell.x), unsigned(cell.y));

        for (const glm::ivec2& offset : neighbours) {
            const glm::ivec2 neighbour = cell + offset;
            if (neighbour.x < 0 || neighbour.y < 0 || neighbour.x >= cols || neighbour.y >= rows) {
                continue;
            }
            const auto to_altitude = dem.cell_altitude(unsigned(neighbour.x), unsigned(neighbour.y));
            const auto to_gradient_vec = dem.cell_gradient_vector(unsigned(neighbour.x), unsigned(neighbour.y));
            if (!to_altitude || !to_gradient_vec) {
                continue;
            }
            const glm::dvec2 to_position = dem.cell_center(unsigned(neighbour.x), unsigned(neighbour.y));
            const glm::dvec2 direction = to_position - from_position;
            const double horizontal = glm::length(direction);
            // metres climbed / descended along the travelled edge; the
            // steepness of the travelled path itself decides which cost curve
            // applies (uphill or downhill)
            const double climb = std::max(0.0, double(*to_altitude) - double(*from_altitude));
            const double descent = std::max(0.0, double(*from_altitude) - double(*to_altitude));
            const double slope_along_degrees = glm::degrees(std::atan((climb + descent) / horizontal));
            double penalty;
            double steep_frac;
            double dist_frac;
            if (climb > 0.0) {
                // climbing: skin-friendly 20-25 degrees is optimal, bootpack
                // territory above 30 degrees
                penalty = uphill_penalty_impl(slope_along_degrees);
                steep_frac = up_steep_frac;
                dist_frac = up_dist_frac;
            } else {
                penalty = gradient_penalty_impl(slope_along_degrees);
                steep_frac = down_steep_frac;
                dist_frac = down_dist_frac;
            }
            // side-tilt: the component of the terrain gradient perpendicular
            // to the travel direction. contouring a steep sidewall must cost
            // extra even when the travelled slope is in the ideal band
            if (cross_weight > 0.0 && horizontal > 0.0) {
                const glm::dvec2 unit = direction / horizontal;
                const glm::dvec2 gradient_vec = 0.5 * (*from_gradient_vec + *to_gradient_vec);
                const double cross_component = std::abs(gradient_vec.x * unit.y - gradient_vec.y * unit.x);
                const double cross_slope_degrees = glm::degrees(std::atan(cross_component));
                penalty += cross_weight * cross_slope_penalty(cross_slope_degrees);
            }

            const double step_cost = dist_frac * horizontal
                + steep_frac * (penalty * horizontal + climb_surcharge * climb);
            const double new_cost = path_cost[size_t(index)] + step_cost;
            const size_t neighbour_index = size_t(to_index(neighbour));
            if (new_cost < path_cost[neighbour_index]) {
                path_cost[neighbour_index] = new_cost;
                came_from[neighbour_index] = index;
                const double estimate = new_cost
                    + std::min(down_dist_frac, up_dist_frac) * glm::length(goal_position - to_position);
                open_set.push({ estimate, int(neighbour_index) });
            }
        }
    }

    if (goal_index < 0) {
        return std::nullopt;
    }

    std::vector<glm::ivec2> path;
    for (int index = goal_index; index >= 0; index = came_from[size_t(index)]) {
        path.push_back({ index % cols, index / cols });
        if (index == to_index(*anchored_start)) {
            break;
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ------------------------------------------------------------------------------------------------
// tile fetching session

namespace {

DemTile decode_tile(const tile::Data& data)
{
    DemTile tile;
    const auto bounds = srs::tile_bounds(data.id);
    tile.min_x = bounds.min.x;
    tile.min_y = bounds.min.y;
    tile.max_x = bounds.max.x;
    tile.max_y = bounds.max.y;

    const auto image = nucleus::utils::image_loader::rgba8(*data.data);
    if (!image) {
        return {};
    }
    tile.width = image->width();
    tile.height = image->height();
    tile.heights.reserve(size_t(tile.width) * tile.height);
    for (unsigned row = 0; row < tile.height; ++row) {
        for (unsigned col = 0; col < tile.width; ++col) {
            tile.heights.push_back(radix::height_encoding::to_float(glm::u8vec3(image->pixel({ col, row }))));
        }
    }
    return tile;
}

} // namespace

RouteOptimisationSession::RouteOptimisationSession(std::vector<glm::dvec2> waypoints,
                                                   const OptimisationWeights& weights,
                                                   QObject* parent)
    : QObject(parent)
    , m_waypoints(std::move(waypoints))
    , m_weights(weights)
{
    m_timeout_timer.setSingleShot(true);
    connect(&m_timeout_timer, &QTimer::timeout, this, &RouteOptimisationSession::finish_fetching);
}

RouteOptimisationSession::~RouteOptimisationSession() = default;

void RouteOptimisationSession::start()
{
    // world-space bounding box of the waypoints, padded so the route can
    // detour away from the direct line between the waypoints. the padding is
    // a user-tunable fraction of the route extent (the "search radius")
    std::vector<glm::dvec2> world_waypoints;
    radix::geometry::Aabb2<double> bounds;
    for (const glm::dvec2& waypoint : m_waypoints) {
        const glm::dvec2 world = srs::lat_long_to_world(waypoint);
        world_waypoints.push_back(world);
        bounds.expand_by(world);
    }
    const glm::dvec2 span = bounds.max - bounds.min;
    const glm::dvec2 padding = span * m_weights.search_padding + glm::dvec2(200.0);
    bounds.min -= padding;
    bounds.max += padding;

    // deepest zoom level that keeps the number of requested tiles bounded
    int zoom = k_max_zoom_level;
    tile::Id min_id = srs::world_xy_to_tile_id(bounds.min, unsigned(zoom));
    tile::Id max_id = srs::world_xy_to_tile_id(bounds.max, unsigned(zoom));
    while (zoom > k_min_zoom_level
        && (max_id.coords.x - min_id.coords.x + 1) * (max_id.coords.y - min_id.coords.y + 1) > k_max_tiles) {
        --zoom;
        min_id = srs::world_xy_to_tile_id(bounds.min, unsigned(zoom));
        max_id = srs::world_xy_to_tile_id(bounds.max, unsigned(zoom));
    }
    m_dem_resolution = srs::tile_width(zoom) / double(k_dem_tile_pixels);
    // must use srs::tile_bounds: the world origin is shifted, raw
    // coord * tile_size arithmetic lands in the wrong hemisphere
    m_bounds_min = srs::tile_bounds(min_id).min;
    m_bounds_max = srs::tile_bounds(max_id).max;
    m_world_waypoints = world_waypoints;

    m_service = std::make_unique<tile::TileLoadService>(tile::constants::dem_tile_server_url,
                                                        tile::TileLoadService::UrlPattern::ZXY, ".png");
    connect(m_service.get(), &tile::TileLoadService::load_finished, this, &RouteOptimisationSession::tile_loaded);

    for (unsigned x = min_id.coords.x; x <= max_id.coords.x; ++x) {
        for (unsigned y = min_id.coords.y; y <= max_id.coords.y; ++y) {
            m_pending += 1;
            m_service->load(tile::Id { unsigned(zoom), { x, y } });
        }
    }
    m_timeout_timer.start(k_fetch_timeout_ms);
}

void RouteOptimisationSession::tile_loaded(tile::Data tile)
{
    if (m_fetch_finished) {
        return;
    }
    m_received_tiles.push_back(std::move(tile));
    m_pending -= 1;
    if (m_pending == 0) {
        m_timeout_timer.stop();
        finish_fetching();
    }
}

void RouteOptimisationSession::finish_fetching()
{
    if (m_fetch_finished) {
        return;
    }
    m_fetch_finished = true;
    m_timeout_timer.stop();
    m_pending = 0;
    compute();
}

void RouteOptimisationSession::compute()
{
    const auto waypoints = m_world_waypoints;
    const auto weights = m_weights;
    const double dem_resolution = m_dem_resolution;
    const glm::dvec2 bounds_min = m_bounds_min;
    const glm::dvec2 bounds_max = m_bounds_max;
    auto tiles = std::move(m_received_tiles);

    // tile decoding and a* are too heavy for the gui thread, so this runs on
    // a thread pool where available. the lite WebAssembly Qt is built without
    // the future feature, so it has no Qt Concurrent, and there the
    // computation runs synchronously on the gui thread
    auto compute_result = [waypoints, weights, dem_resolution, bounds_min, bounds_max,
                           tiles = std::move(tiles)]() mutable -> OptimisationResult {
        OptimisationResult result;

        std::vector<DemTile> decoded_tiles;
        decoded_tiles.reserve(tiles.size());
        unsigned good_tiles = 0;
        for (const auto& tile : tiles) {
            if (tile.network_info.status != tile::NetworkInfo::Status::Good || !tile.data) {
                continue;
            }
            auto decoded = decode_tile(tile);
            if (decoded.width == 0) {
                continue;
            }
            decoded_tiles.push_back(std::move(decoded));
            ++good_tiles;
        }
        if (good_tiles == 0) {
            result.status = "No terrain data available for this area";
            return result;
        }

        auto raster = DemRaster::create(decoded_tiles, bounds_min, bounds_max, dem_resolution);
        if (!raster) {
            result.status = "No terrain data available for this area";
            return result;
        }

        for (size_t i = 1; i < waypoints.size(); ++i) {
            const glm::dvec2 from = waypoints[i - 1];
            const glm::dvec2 to = waypoints[i];
            const auto to_cell = glm::ivec2(int((to.x - raster->cell_center(0, 0).x) / raster->cell_size()),
                                            int((to.y - raster->cell_center(0, 0).y) / raster->cell_size()));
            const auto from_cell = glm::ivec2(int((from.x - raster->cell_center(0, 0).x) / raster->cell_size()),
                                              int((from.y - raster->cell_center(0, 0).y) / raster->cell_size()));

            // optimise_segment anchors the cells to the nearest valid cells itself
            const auto path = optimise_segment(*raster, from_cell, to_cell, weights.steepness, weights.distance,
                weights.cross_slope / 100.0 * 2.0, weights.uphill_steepness, weights.uphill_penalty);
            if (!path || path->size() < 2) {
                result.status = "No valid route found between waypoints (too steep or missing data)";
                return result;
            }

            std::vector<glm::dvec3> segment;
            segment.reserve(path->size() + 2);
            // segment points are (lat, lon, altitude in metres). the raster
            // altitude is already true metres - it must not go through
            // world_to_lat_long_alt, whose z is the mercator-stretched world
            // z (metres / cos(lat))
            // altitudes from the exact rendered surface (triangle mesh), so
            // the line sits on the terrain the gpu draws, also diagonal to
            // the grid
            const auto add_world_point = [&](glm::dvec2 world) {
                const glm::dvec2 lat_long = srs::world_to_lat_long(world);
                std::optional<float> altitude;
                for (const DemTile& tile : decoded_tiles) {
                    altitude = tile.mesh_sample(world.x, world.y);
                    if (altitude) {
                        break;
                    }
                }
                segment.push_back({ lat_long.x, lat_long.y, altitude.value_or(0.0) });
            };

            add_world_point(from); // start at the exact clicked waypoint
            for (const glm::ivec2& cell : *path) {
                add_world_point(raster->cell_center(unsigned(cell.x), unsigned(cell.y)));
            }
            add_world_point(to); // end at the exact clicked waypoint

            // light smoothing of the world-space path (the altitude is
            // resampled from the raster, so it stays dem-accurate)
            constexpr int radius = 2;
            if (segment.size() > size_t(2 * radius + 1)) {
                std::vector<glm::dvec3> smoothed = segment;
                for (size_t j = size_t(radius); j + radius < segment.size(); ++j) {
                    glm::dvec2 position_sum(0.0);
                    double altitude_sum = 0.0;
                    for (int k = -radius; k <= radius; ++k) {
                        const auto& p = segment[j - size_t(k)];
                        position_sum += glm::dvec2(p.x, p.y);
                        altitude_sum += p.z;
                    }
                    smoothed[j].x = position_sum.x / double(2 * radius + 1);
                    smoothed[j].y = position_sum.y / double(2 * radius + 1);
                    smoothed[j].z = altitude_sum / double(2 * radius + 1);
                }
                segment = smoothed;
            }

            result.segment_paths.push_back(std::move(segment));
        }

        result.success = true;
        result.status = "Optimized";
        return result;
    };

#ifndef __EMSCRIPTEN__
    auto* watcher = new QFutureWatcher<OptimisationResult>(this);
    connect(watcher, &QFutureWatcher<OptimisationResult>::finished, this, [this, watcher]() {
        emit finished(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(std::move(compute_result)));
#else
    emit finished(compute_result());
#endif
}

} // namespace nucleus::planner
