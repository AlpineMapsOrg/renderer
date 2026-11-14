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

#include "PlannerModel.h"

#include "RenderThreadNotifier.h"
#include "RenderingContext.h"
#ifndef __EMSCRIPTEN__
// the lite WebAssembly Qt is built without the future feature, so it has no QFutureWatcher / Qt Concurrent
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#endif
#include <QFile>
#include <QFileInfo>
#include <gl_engine/Context.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/planner/RouteOptimizer.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/cache_quieries.h>
#include <nucleus/srs.h>

namespace {
// dem cell size fallback (metres) for interpolation steps when no dem tile
// is cached for a segment: normally the samples are spaced at one cell of
// the tile the renderer displays there (see compute_render_data)
constexpr double k_fallback_cell_size = 20.0;
// upper bound of interpolation steps per segment, so that a segment across
// whole austria does not produce tens of thousands of samples
constexpr int k_max_steps_per_segment = 500;
// elevation smoothing window of the sampled route line, in samples per side
constexpr int k_elevation_smoothing_radius = 2;
// length of the tiny marker segment at each route point, in metres. the
// capsule shader renders the short segment as a round dot (the cap
// hemispheres dominate). horizontal, so the ribbon quad stays valid when the
// camera looks straight down (a vertical segment would degenerate there)
constexpr double k_marker_length = 1.0;
constexpr double k_metres_per_degree_latitude = 111320.0;
// the user position dot floats this many metres above the dem, so that it is
// not swallowed by the terrain
constexpr double k_user_position_z_offset = 3.0;
// the user position dot is re-placed at this interval, so its dem altitude
// follows the currently displayed terrain while it is active
constexpr int k_user_position_refresh_interval_ms = 1000;
} // namespace

PlannerModel::PlannerModel(QObject* parent)
    : QObject(parent)
{
    auto* ctx = RenderingContext::instance();
    connect(ctx, &RenderingContext::initialised, this, [this, ctx]() {
        auto* track_manager = ctx->engine_context()->track_manager();
        connect(this, &PlannerModel::planned_track_changed, track_manager, &gl_engine::TrackManager::change_planned_track);
        connect(this, &PlannerModel::planned_track_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);
        connect(this, &PlannerModel::user_position_changed, track_manager, &gl_engine::TrackManager::change_user_position);
        connect(this, &PlannerModel::user_position_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);
        connect(this, &PlannerModel::user_position_cleared, track_manager, &gl_engine::TrackManager::clear_user_position);
        connect(this, &PlannerModel::route_line_visible_changed, track_manager, &gl_engine::TrackManager::change_planned_line_visible);
        connect(this, &PlannerModel::user_position_cleared, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);

        m_data_querier = ctx->data_querier();
    });

    m_user_position_timer.setInterval(k_user_position_refresh_interval_ms);
    connect(&m_user_position_timer, &QTimer::timeout, this, &PlannerModel::place_user_position);
}

QVariantList PlannerModel::points() const
{
    QVariantList list;
    int index = 0;
    for (const auto& segment : m_plan.track) {
        for (const auto& point : segment) {
            list.append(QVariantMap {
                { "index", index++ },
                { "latitude", point.latitude },
                { "longitude", point.longitude },
                { "elevation", point.elevation },
            });
        }
    }
    return list;
}

QString PlannerModel::name() const { return m_plan.name; }

void PlannerModel::set_name(const QString& new_name)
{
    if (m_plan.name == new_name) {
        return;
    }
    m_plan.name = new_name;
    emit name_changed();
}

QString PlannerModel::description() const { return m_plan.description; }

void PlannerModel::set_description(const QString& new_description)
{
    if (m_plan.description == new_description) {
        return;
    }
    m_plan.description = new_description;
    emit description_changed();
}

double PlannerModel::distance() const { return m_plan.distance; }
double PlannerModel::ascent() const { return m_plan.ascent; }
double PlannerModel::descent() const { return m_plan.descent; }
double PlannerModel::avg_steepness() const { return m_avg_steepness; }
double PlannerModel::max_steepness() const { return m_max_steepness; }

int PlannerModel::steepness_weight() const { return m_steepness_weight; }

void PlannerModel::set_steepness_weight(int weight)
{
    if (m_steepness_weight == weight) {
        return;
    }
    m_steepness_weight = weight;
    emit optimisation_parameters_changed();
}

int PlannerModel::distance_weight() const { return m_distance_weight; }

void PlannerModel::set_distance_weight(int weight)
{
    if (m_distance_weight == weight) {
        return;
    }
    m_distance_weight = weight;
    emit optimisation_parameters_changed();
}

int PlannerModel::cross_slope_weight() const { return m_cross_slope_weight; }

void PlannerModel::set_cross_slope_weight(int weight)
{
    if (m_cross_slope_weight == weight) {
        return;
    }
    m_cross_slope_weight = weight;
    emit optimisation_parameters_changed();
}

int PlannerModel::search_radius() const { return m_search_radius; }

void PlannerModel::set_search_radius(int percent)
{
    percent = std::clamp(percent, 5, 150);
    if (m_search_radius == percent) {
        return;
    }
    m_search_radius = percent;
    emit optimisation_parameters_changed();
}

bool PlannerModel::optimisation_running() const { return m_optimisation_running; }

bool PlannerModel::route_line_visible() const { return m_route_line_visible; }

QVariantList PlannerModel::elevation_profile() const
{
    QVariantList profile;
    double distance = 0.0;
    const nucleus::track::Point* previous = nullptr;
    for (const auto& segment : m_render_line.track) {
        for (const auto& point : segment) {
            double slope_degrees = 0.0;
            if (previous) {
                const nucleus::track::Point step = { previous->latitude, previous->longitude, 0.0 };
                const nucleus::track::Point step_to = { point.latitude, point.longitude, 0.0 };
                double horizontal = m_plan.haversine_distance(step, step_to);
                const double dz = std::abs(point.elevation - previous->elevation);
                if (horizontal <= 0.0) {
                    horizontal = 0.001;
                }
                slope_degrees = glm::degrees(std::atan2(dz, horizontal));
            }
            profile.append(QVariantMap {
                { "x", distance },
                { "y", point.elevation },
                { "slope", slope_degrees },
            });
            if (previous) {
                const nucleus::track::Point from = { previous->latitude, previous->longitude, 0.0 };
                const nucleus::track::Point to = { point.latitude, point.longitude, 0.0 };
                distance += m_plan.haversine_distance(from, to);
            }
            previous = &point;
        }
    }
    return profile;
}

void PlannerModel::set_route_line_visible(bool visible)
{
    if (m_route_line_visible == visible) {
        return;
    }
    m_route_line_visible = visible;
    emit route_line_visible_changed(visible);
}

QString PlannerModel::status() const { return m_status; }

void PlannerModel::invalidate_optimisation()
{
    m_optimised_segments.clear();
    m_optimisation_generation += 1; // discards in-flight optimisation results
    if (!m_status.isEmpty()) {
        m_status.clear();
        emit status_changed();
    }
}

void PlannerModel::clear_optimised_segment(unsigned pair_index)
{
    // clear (not erase): a move keeps the plan's pair count, and the vector
    // must stay aligned with the pair numbering of update_render_data
    if (pair_index < m_optimised_segments.size()) {
        m_optimised_segments[pair_index].clear();
    }
}

void PlannerModel::erase_optimised_segment(unsigned pair_index)
{
    if (pair_index < m_optimised_segments.size()) {
        m_optimised_segments.erase(m_optimised_segments.begin() + long(pair_index));
    }
}

void PlannerModel::drop_inflight_result()
{
    m_optimisation_generation += 1; // discards in-flight optimisation results
}

void PlannerModel::clear_status()
{
    if (!m_status.isEmpty()) {
        m_status.clear();
        emit status_changed();
    }
}

std::vector<glm::dvec2> PlannerModel::waypoint_sequence() const
{
    std::vector<glm::dvec2> sequence;
    for (const auto& segment : m_plan.track) {
        for (const auto& point : segment) {
            sequence.emplace_back(point.latitude, point.longitude);
        }
    }
    return sequence;
}

void PlannerModel::add_point_at(double latitude, double longitude, double altitude)
{
    m_plan.add_new_point({ latitude, longitude, altitude });
    m_plan.recompute_stats();
    // appending a point adds one new waypoint pair; the optimised paths of
    // all existing pairs stay valid and keep rendering. only the new pair
    // falls back to the approximate straight line until the next optimise
    drop_inflight_result();
    clear_status();
    update_render_data();
    emit points_changed();
}

void PlannerModel::delete_point(int index)
{
    const unsigned count_before = m_plan.point_count();
    m_plan.delete_point(unsigned(index));
    if (m_plan.point_count() == count_before) {
        return; // index out of range, nothing was deleted
    }
    m_plan.recompute_stats();
    // deleting waypoint w destroys its adjacent pairs (w-1,w) and (w,w+1);
    // the cached paths before w-1 and from w+1 on stay valid (from w+1 on
    // they shift one index down). when both neighbours exist, the merged
    // pair (w-1,w+1) is new and keeps an empty entry, so the vector stays
    // aligned with the pair numbering of update_render_data. the plan is
    // one continuous leg (imports are flattened), so the ui index is both
    // the flat waypoint index and the pair numbering base
    const unsigned w = unsigned(index);
    if (w > 0 && w + 1 < count_before) {
        // interior waypoint: erase the two dead neighbours (the second one
        // has shifted into the first one's place) and re-insert the empty
        // placeholder of the merged pair
        erase_optimised_segment(w - 1);
        erase_optimised_segment(w - 1);
        if (w - 1 < m_optimised_segments.size()) {
            m_optimised_segments.insert(m_optimised_segments.begin() + long(w - 1), {});
        }
    } else {
        // first or last waypoint: only the single adjacent pair dies
        erase_optimised_segment(w > 0 ? w - 1 : 0);
    }
    drop_inflight_result();
    clear_status();
    update_render_data();
    emit points_changed();
}

void PlannerModel::move_point_up(int index)
{
    const unsigned flat = unsigned(index);
    if (m_plan.move_point(unsigned(index), -1) == unsigned(index)) {
        return; // boundary, the move was a no-op
    }
    // the moved point (now at flat-1) and its former neighbour (now at flat)
    // both changed partners: the pairs flat-2, flat-1 and flat have new
    // geometry or reversed direction and fall back to the straight line
    // until the next optimise
    if (flat >= 2) {
        clear_optimised_segment(flat - 2);
    }
    clear_optimised_segment(flat - 1);
    clear_optimised_segment(flat);
    drop_inflight_result();
    clear_status();
    update_render_data();
    emit points_changed();
}

void PlannerModel::move_point_down(int index)
{
    const unsigned flat = unsigned(index);
    if (m_plan.move_point(unsigned(index), 1) == unsigned(index)) {
        return; // boundary, the move was a no-op
    }
    // the moved point (now at flat+1) and its former neighbour (now at flat)
    // both changed partners: the pairs flat-1, flat and flat+1 have new
    // geometry or reversed direction and fall back to the straight line
    // until the next optimise
    if (flat >= 1) {
        clear_optimised_segment(flat - 1);
    }
    clear_optimised_segment(flat);
    clear_optimised_segment(flat + 1);
    drop_inflight_result();
    clear_status();
    update_render_data();
    emit points_changed();
}

void PlannerModel::reset()
{
    m_plan.reset();
    invalidate_optimisation();
    update_render_data();
    emit points_changed();
    emit name_changed();
    emit description_changed();
}

void PlannerModel::export_gpx(const QUrl& file_url)
{
    if (m_plan.point_count() == 0) {
        qDebug("PlannerModel::export_gpx: nothing to export");
        return;
    }
    // when an optimised route exists, export the dense computed line (the
    // route that is rendered), otherwise the clicked waypoint plan
    nucleus::track::Gpx route = !m_optimised_segments.empty() ? m_render_line : m_plan;
    route.creationDateTime = QDateTime::currentDateTime();
    // android file dialogs return content:// uris, which QFile can open but
    // which have no local file path
    const QString path = file_url.isLocalFile() ? file_url.toLocalFile() : file_url.toString();
    if (!route.save(path)) {
        m_status = tr("Could not write the GPX file");
        emit status_changed();
    }
}

void PlannerModel::import_gpx(const QUrl& file_url)
{
    const QString path = file_url.isLocalFile() ? file_url.toLocalFile() : file_url.toString();
    if (path.isEmpty()) {
        qWarning() << "PlannerModel::import_gpx: not a local file" << file_url;
        m_status = tr("Could not open the selected file");
        emit status_changed();
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "PlannerModel::import_gpx: failed to open file!" << path;
        m_status = tr("Could not open the selected file");
        emit status_changed();
        return;
    }

    QXmlStreamReader xml_reader(&file);
    std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xml_reader);
    file.close();

    if (gpx == nullptr) {
        qWarning() << "PlannerModel::import_gpx: could not parse GPX file!" << path;
        m_status = tr("Could not parse the GPX file");
        emit status_changed();
        return;
    }
    if (gpx->point_count() == 0) {
        m_status = tr("The GPX file contains no track points");
        emit status_changed();
        return;
    }

    // the imported track becomes the editable plan (replaces the current one).
    // the planner edits one continuous leg and lists the points flat, so
    // multi-segment tracks are merged into a single segment: that keeps the
    // ui indices, the optimiser's pair numbering and the rendered line
    // consistent (segments of a gpx file are not connected, but a planned
    // route continues across former leg boundaries)
    m_plan = std::move(*gpx);
    m_plan.flatten_segments();
    if (m_plan.name.isEmpty()) {
        m_plan.name = QFileInfo(path).completeBaseName();
    }
    m_plan.recompute_stats();
    invalidate_optimisation();
    update_render_data();
    emit points_changed();
    // the imported metadata replaces the current one
    emit name_changed();
    emit description_changed();

    // move the camera to the start of the imported track, like the track
    // upload button does
    for (const auto& segment : m_plan.track) {
        if (segment.empty()) {
            continue;
        }
        emit track_imported({ segment.front().latitude, segment.front().longitude });
        break;
    }
}

void PlannerModel::optimise_route()
{
    if (m_optimisation_running) {
        return;
    }
    const auto waypoints = waypoint_sequence();
    if (waypoints.size() < 2) {
        return;
    }

    m_optimisation_running = true;
    emit optimisation_running_changed();
    m_status = tr("Optimizing…");
    emit status_changed();
    const unsigned generation = ++m_optimisation_generation;

    nucleus::planner::OptimisationWeights weights;
    weights.steepness = m_steepness_weight;
    weights.distance = m_distance_weight;
    weights.cross_slope = m_cross_slope_weight;
    weights.search_padding = m_search_radius / 100.0;

    auto* session = new nucleus::planner::RouteOptimisationSession(waypoints, weights, this);
    connect(session, &nucleus::planner::RouteOptimisationSession::finished, this,
            [this, generation, session](const nucleus::planner::OptimisationResult& result) {
                session->deleteLater();
                if (generation != m_optimisation_generation) {
                    // the result is stale, but the optimisation itself did
                    // finish: without resetting here the ui stays stuck in
                    // the running state (edited a waypoint while optimizing)
                    m_optimisation_running = false;
                    emit optimisation_running_changed();
                    clear_status();
                    return;
                }
                optimisation_finished(result);
            });
    session->start();
}

void PlannerModel::undo_last_point()
{
    if (m_plan.point_count() == 0) {
        return;
    }
    // removing the last waypoint only drops the final pair
    const unsigned last = m_plan.point_count() - 1;
    if (last > 0) {
        erase_optimised_segment(last - 1);
    }
    m_plan.delete_last_point();
    m_plan.recompute_stats();
    drop_inflight_result();
    clear_status();
    update_render_data();
    emit points_changed();
}

void PlannerModel::fill_preview_route()
{
    if (m_render_line.point_count() < 2) {
        return;
    }
    std::vector<glm::dvec3> world_points;
    world_points.reserve(m_render_line.point_count());
    for (const auto& segment : m_render_line.track) {
        for (const auto& point : segment) {
            // srs world z is the mercator-stretched altitude (metres/cos(lat))
            world_points.push_back(nucleus::srs::lat_long_alt_to_world({ point.latitude, point.longitude, point.elevation }));
        }
    }

    RenderingContext::instance()->set_preview_route_points(world_points);
}

void PlannerModel::optimisation_finished(const nucleus::planner::OptimisationResult& result)
{
    m_optimisation_running = false;
    emit optimisation_running_changed();
    m_status = result.status;
    emit status_changed();

    if (!result.success) {
        m_optimised_segments.clear();
        update_render_data();
        emit points_changed();
        return;
    }

    // distribute the optimised segment paths onto the waypoint pairs
    m_optimised_segments.assign(result.segment_paths.size(), {});
    size_t pair = 0;
    for (const auto& segment : result.segment_paths) {
        nucleus::track::Segment points;
        points.reserve(segment.size());
        for (const auto& lat_long_alt : segment) {
            points.push_back({ lat_long_alt.x, lat_long_alt.y, lat_long_alt.z });
        }
        m_optimised_segments[pair++] = std::move(points);
    }
    update_render_data();
    emit points_changed();
}

void PlannerModel::set_user_position(double latitude, double longitude, double altitude)
{
    if (latitude == 0.0 && longitude == 0.0) {
        return;
    }
    m_user_position_latitude = latitude;
    m_user_position_longitude = longitude;
    m_user_position_altitude = altitude;
    m_user_position_timer.start();
    place_user_position();
}

void PlannerModel::clear_user_position()
{
    m_user_position_timer.stop();
    emit user_position_cleared();
}

void PlannerModel::place_user_position()
{
    // place the dot on the terrain: the dem altitude is more reliable than
    // the gnss altitude; the gnss altitude is only the fallback
    double dem_altitude = m_user_position_altitude;
    if (m_data_querier) {
        const std::expected<float, QString> queried = m_data_querier->get_altitude({ m_user_position_latitude, m_user_position_longitude });
        if (queried.has_value()) {
            dem_altitude = queried.value();
        }
    }

    // same dot style as the route point markers (~1m segment rendered as a
    // sphere by the capsule shader), horizontal so it survives top-down views,
    // lifted above the terrain so it is not swallowed by the dem
    const double lat_offset = k_marker_length / k_metres_per_degree_latitude;
    nucleus::track::Gpx marker;
    marker.add_new_point({ m_user_position_latitude - lat_offset / 2.0, m_user_position_longitude, dem_altitude + k_user_position_z_offset });
    marker.add_new_point({ m_user_position_latitude + lat_offset / 2.0, m_user_position_longitude, dem_altitude + k_user_position_z_offset });
    emit user_position_changed(marker);
}

namespace {

struct RenderData {
    nucleus::track::Gpx line;
    nucleus::track::Gpx markers;
    double avg_steepness = 0;
    double max_steepness = 0;
};

// builds the dense, dem-interpolated render line of the plan. runs on a
// thread pool (see update_render_data) and touches only its arguments: the
// plan/optimised-segment copies and the tile cache, which is internally
// synchronised
RenderData compute_render_data(const nucleus::track::Gpx& plan,
                               const std::vector<std::vector<nucleus::track::Point>>& optimised_segments,
                               nucleus::tile::MemoryCache* cache)
{
    RenderData result;

    // accumulates steepness statistics along the rendered line. called for
    // every point appended to the line, in order; the chain restarts at each
    // plan segment (segments are not connected to each other)
    double total_abs_elevation_change = 0;
    double total_distance = 0;
    bool chain_started = false;
    nucleus::track::Point chain_previous {};
    const auto add_line_point = [&](const nucleus::track::Point& point) {
        if (chain_started) {
            const double dist = plan.haversine_distance(chain_previous, point);
            if (dist > 0) {
                const double elevation_change = std::abs(point.elevation - chain_previous.elevation);
                total_abs_elevation_change += elevation_change;
                total_distance += dist;
                result.max_steepness = std::max(result.max_steepness, glm::degrees(std::atan(elevation_change / dist)));
            }
        } else {
            chain_started = true;
        }
        chain_previous = point;
        result.line.add_new_point(point);
    };
    const auto restart_chain = [&chain_started]() { chain_started = false; };

    // decodes each touched dem tile once instead of once per sample
    nucleus::tile::cache_queries::DemMeshSampler sampler(cache);

    size_t pair_index = 0; // consecutive waypoint pair counter across plan segments

    for (const auto& segment : plan.track) {
        restart_chain();

        // collect the interpolated samples of this plan segment, then smooth
        // their elevations, so that the rendered steepness changes gradually
        std::vector<nucleus::track::Point> line_points;
        line_points.reserve(segment.size() * 8);
        for (size_t i = 0; i < segment.size(); ++i) {
            const nucleus::track::Point& a = segment[i];
            line_points.push_back(a);

            // point marker: ~1 m segment that the capsule shader renders as a
            // round dot sitting on the route point
            const double lat_offset = k_marker_length / k_metres_per_degree_latitude;
            result.markers.add_new_segment();
            result.markers.add_new_point({ a.latitude - lat_offset / 2.0, a.longitude, a.elevation });
            result.markers.add_new_point({ a.latitude + lat_offset / 2.0, a.longitude, a.elevation });

            if (i + 1 >= segment.size()) {
                continue;
            }
            const nucleus::track::Point& b = segment[i + 1];

            // use the optimised least-cost path for this waypoint pair when
            // available, otherwise interpolate the straight line, sampling
            // the dem for the elevation
            std::vector<nucleus::track::Point> straight_samples;
            const std::vector<nucleus::track::Point>* path = &straight_samples;
            if (pair_index < optimised_segments.size() && !optimised_segments[pair_index].empty()) {
                path = &optimised_segments[pair_index];
            } else {
                const glm::dvec2 world_a = nucleus::srs::lat_long_to_world({ a.latitude, a.longitude });
                const glm::dvec2 world_b = nucleus::srs::lat_long_to_world({ b.latitude, b.longitude });
                const double dist = glm::distance(world_a, world_b);

                // one interpolation step spans roughly one dem cell of the
                // terrain displayed there, so the rendered line matches the
                // resolution of the drawn tiles without oversampling; the
                // tile at the segment midpoint decides the cell size
                double cell_size = k_fallback_cell_size;
                if (const auto dem_cell_size = sampler.cell_size({ (a.latitude + b.latitude) / 2.0, (a.longitude + b.longitude) / 2.0 })) {
                    cell_size = dem_cell_size.value();
                }
                const int steps = std::min(k_max_steps_per_segment,
                    std::max(1, int(std::ceil(dist / cell_size))));

                for (int s = 1; s < steps; ++s) {
                    const double t = double(s) / double(steps);
                    const double lat = a.latitude + (b.latitude - a.latitude) * t;
                    const double lon = a.longitude + (b.longitude - a.longitude) * t;

                    double alt = a.elevation + (b.elevation - a.elevation) * t;
                    // sample the exact rendered surface (triangle mesh), so
                    // the line matches the drawn terrain, also diagonal to
                    // the grid
                    const std::expected<float, QString> dem_altitude = sampler.altitude({ lat, lon });
                    if (dem_altitude.has_value()) {
                        alt = dem_altitude.value();
                    }
                    straight_samples.push_back({ lat, lon, alt });
                }
            }
            ++pair_index;

            // the path endpoints are the exact waypoints themselves; waypoint b
            // is appended by the next loop iteration (or as the segment end)
            for (size_t k = 0; k + 1 < path->size(); ++k) {
                line_points.push_back((*path)[k]);
            }
        }

        // moving average over the sampled elevations (route points keep their
        // exact elevation at the segment ends)
        const int radius = k_elevation_smoothing_radius;
        for (size_t i = radius; i + radius < line_points.size(); ++i) {
            double sum = 0;
            for (int k = -radius; k <= radius; ++k) {
                sum += line_points[size_t(int(i) + k)].elevation;
            }
            line_points[i].elevation = sum / double(2 * radius + 1);
        }

        for (const nucleus::track::Point& point : line_points) {
            add_line_point(point);
        }
    }

    // average steepness: atan of the mean absolute slope along the whole route
    if (total_distance > 0) {
        result.avg_steepness = glm::degrees(std::atan(total_abs_elevation_change / total_distance));
    }
    return result;
}

} // namespace

void PlannerModel::update_render_data()
{
    if (m_plan.point_count() == 0) {
        m_render_data_generation += 1; // discard an in-flight computation
        m_render_line = {};
        m_render_markers = {};
        m_avg_steepness = 0;
        m_max_steepness = 0;
        emit planned_track_changed(m_render_line, m_render_markers);
        emit render_data_changed();
        return;
    }

    // snapshot the inputs on the gui thread, the worker touches only the
    // copies (and the tile cache, which is internally synchronised and lives
    // for the app's lifetime). the generation discards results of superseded
    // snapshots when the plan changes again while a computation is in flight
    const unsigned generation = ++m_render_data_generation;
    nucleus::track::Gpx plan = m_plan;
    auto optimised_segments = m_optimised_segments;
    nucleus::tile::MemoryCache* cache = &RenderingContext::instance()->geometry_scheduler()->ram_cache();

    auto compute_job = [plan = std::move(plan), optimised = std::move(optimised_segments), cache]() {
        return compute_render_data(plan, optimised, cache);
    };

#ifndef __EMSCRIPTEN__
    // the dem sampling runs on a thread pool. the lite WebAssembly Qt is
    // built without the future feature, so it has no Qt Concurrent, and
    // there the computation runs synchronously on the gui thread
    auto* watcher = new QFutureWatcher<RenderData>(this);
    connect(watcher, &QFutureWatcher<RenderData>::finished, this,
        [this, generation, watcher]() {
            watcher->deleteLater();
            if (generation != m_render_data_generation) {
                return; // the plan changed while computing
            }
            const RenderData& result = watcher->result();
            m_render_line = result.line;
            m_render_markers = result.markers;
            m_avg_steepness = result.avg_steepness;
            m_max_steepness = result.max_steepness;
            emit planned_track_changed(m_render_line, m_render_markers);
            emit render_data_changed();
        });
    watcher->setFuture(QtConcurrent::run(std::move(compute_job)));
#else
    Q_UNUSED(generation);
    const RenderData result = compute_job();
    m_render_line = result.line;
    m_render_markers = result.markers;
    m_avg_steepness = result.avg_steepness;
    m_max_steepness = result.max_steepness;
    emit planned_track_changed(m_render_line, m_render_markers);
    emit render_data_changed();
#endif
}
