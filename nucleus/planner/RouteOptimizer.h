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

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace nucleus::tile {
class TileLoadService;
struct Data;
}

namespace nucleus::planner {

struct OptimisationResult {
    // optimised path per consecutive waypoint pair, in geo coordinates
    // (lat, long, altitude)
    std::vector<std::vector<glm::dvec3>> segment_paths;
    bool success = false;
    QString status;
};

struct OptimisationWeights {
    double steepness = 60.0;         // 0..100, downhill edges
    double uphill_steepness = 60.0;  // 0..100, climbing edges
    double distance = 40.0;          // 0..100
    double cross_slope = 50.0;       // 0..100, scales the across-travel-direction slope penalty
    double uphill_penalty = 5.0;     // 0..10 penalty metres per vertical metre climbed
    double search_padding = 0.2;     // fraction of the route extent added as search margin
};

// cost of DESCENDING terrain with the given slope angle, in penalty metres
// per metre travelled (0 = perfect ski descent gradient, high = avoid).
// steep ski descent preferences: 30-45 degrees is ideal, up to 60 degrees
// is fine, above 60 degrees requires a rappel and is heavily penalised,
// below 20 degrees is avoided (flat traversing).
double gradient_penalty(double slope_degrees);

// cost of CLIMBING terrain with the given slope angle, in penalty metres per
// metre travelled: 20-25 degrees is optimal (skin-friendly), 15-20 degrees is
// fine, flatter is suboptimal, above 30 degrees the skins come off (bootpack)
// and the cost rises steeply
double uphill_penalty(double slope_degrees);

// penalty for the slope component perpendicular to the travel direction
// (side-tilt). contouring a steep sidewall is uncomfortable and dangerous
// even when the terrain steepness itself is in the ideal band, so this curve
// rises with the cross slope from zero, unlike gradient_penalty
double cross_slope_penalty(double cross_slope_degrees);

// decoded dem tile in web-mercator metres
struct DemTile {
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    unsigned width = 0, height = 0;
    std::vector<float> heights; // row-major, row 0 is the north edge

    std::optional<float> sample(double world_x, double world_y) const;
    // altitude of the *rendered* terrain surface: the tile mesh is a triangle
    // strip over the texel grid (diagonal from the south-western to the
    // north-eastern cell corner, see terrain_mesh_index_generator), so the
    // surface between four texels is two triangles, not a bilinear patch.
    // sampling this instead of a bilinear patch removes the residual wiggle
    // of the rendered line when travelling diagonal to the grid
    std::optional<float> mesh_sample(double world_x, double world_y) const;
};

// local metric dem grid over the planning area, sampled from decoded tiles
class DemRaster {
public:
    // blits the tiles into a grid with the given cell size over the bounding
    // box (tiles must all be from the same zoom level). returns nullopt if
    // no tile contributes data
    static std::optional<DemRaster> create(const std::vector<DemTile>& tiles,
                                           glm::dvec2 bbox_min, glm::dvec2 bbox_max,
                                           double cell_size);

    unsigned cols() const { return m_cols; }
    unsigned rows() const { return m_rows; }
    double cell_size() const { return m_cell_size; }
    glm::dvec2 cell_center(unsigned col, unsigned row) const;
    std::optional<float> cell_altitude(unsigned col, unsigned row) const;
    // terrain slope of the cell in degrees, from central differences of the
    // dem grid (one-sided at the borders). nullopt for no-data cells
    std::optional<float> cell_gradient(unsigned col, unsigned row) const;
    // terrain gradient vector (dz/dx, dz/dy), nullopt for no-data cells.
    // needed to split the slope into along-travel and across-travel parts
    std::optional<glm::dvec2> cell_gradient_vector(unsigned col, unsigned row) const;
    std::optional<float> altitude(glm::dvec2 world) const; // bilinear between cell centres
    bool inside(glm::dvec2 world) const;

private:
    glm::dvec2 m_min = {};
    double m_cell_size = 10.0;
    unsigned m_cols = 0, m_rows = 0;
    std::vector<float> m_altitudes; // NaN = no data
    std::vector<float> m_gradients; // terrain slope in degrees, NaN = no data
    std::vector<glm::vec2> m_gradient_xy; // (dz/dx, dz/dy), NaN = no data
};

// least-cost path between two grid cells (8-connected, a*). the cost prices
// the slope of the travelled edge itself, with the direction deciding the
// curve: climbing edges use the uphill curve and the uphill steepness
// weight, descending edges the downhill curve and the downhill steepness
// weight (the distance weight is shared by both mixes). the slope across the
// travel direction (side-tilt, scaled by cross_slope_weight) and a surcharge
// per vertical metre climbed (climb_penalty) are added on top. returns
// nullopt if no path could be found
std::optional<std::vector<glm::ivec2>> optimise_segment(const DemRaster& dem,
                                                        glm::ivec2 start, glm::ivec2 goal,
                                                        double steepness_weight, double distance_weight,
                                                        double cross_slope_weight = 0.0,
                                                        double uphill_steepness_weight = 60.0,
                                                        double climb_penalty = 5.0);

// fetches dem tiles for the planning area (highest resolution available for
// the area size), decodes them and computes the least-cost path between
// consecutive waypoints. runs the heavy computation on a thread pool and
// reports the result via the finished signal (on the thread start() was
// called from)
class RouteOptimisationSession : public QObject {
    Q_OBJECT
public:
    RouteOptimisationSession(std::vector<glm::dvec2> waypoints, // lat, lon
                             const OptimisationWeights& weights,
                             QObject* parent = nullptr);
    ~RouteOptimisationSession() override;
    void start();

signals:
    void finished(const nucleus::planner::OptimisationResult& result);

private:
    void tile_loaded(tile::Data tile);
    void finish_fetching();
    void compute();

    std::unique_ptr<tile::TileLoadService> m_service;
    QTimer m_timeout_timer;
    std::vector<glm::dvec2> m_waypoints; // lat, lon
    OptimisationWeights m_weights;
    std::vector<glm::dvec2> m_world_waypoints; // web-mercator metres
    glm::dvec2 m_bounds_min = {};
    glm::dvec2 m_bounds_max = {};
    double m_dem_resolution = 10.0;
    std::vector<tile::Data> m_received_tiles;
    unsigned m_pending = 0;
    bool m_fetch_finished = false;
};

} // namespace nucleus::planner
