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
#include <QDir>
#include <QStandardPaths>
#include <gl_engine/Context.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/planner/RouteOptimizer.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/cache_quieries.h>
#include <nucleus/srs.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
// webassembly: the routes directory lives in the idbfs mount (persisted to
// IndexedDB), but qt never flushes it on its own - every write is followed
// by a syncfs(false) and the routes saved by a previous session are pulled
// back with a syncfs(true) at startup, whose callback refreshes the list

static PlannerModel* alpine_app_wasm_planner = nullptr;

EM_JS(void, alpine_app_routes_fs_sync_to_idb, (), {
    FS.syncfs(false, function(err) {
        if (err)
            console.error('Failed to persist routes', err);
    });
});

// the idbfs mount uses EM_ASM (not EM_JS): emscripten scans EM_ASM bodies
// for js library symbols, so IDBFS from -lidbfs.js is resolved here; an
// EM_JS body would leave it undefined at runtime
static void alpine_app_routes_fs_init()
{
    EM_ASM({
        // nothing is persisted unless an idbfs filesystem exists: mount it,
        // create the routes directory inside it and pull what a previous
        // session saved out of IndexedDB
        try {
            FS.mkdir('/idbfs'); // the mount point must exist before mounting
        } catch (e) {} // already exists
        try {
            FS.mount(IDBFS, {}, '/idbfs');
        } catch (e) {
            console.error('Failed to mount idbfs', e && (e.message || e.errno || e));
            return;
        }
        try {
            FS.mkdir('/idbfs/routes');
        } catch (e) {} // already exists
        FS.syncfs(true, function(err) {
            if (err) {
                console.error('Failed to load persisted routes', err);
                return;
            }
            _alpine_app_routes_fs_synced();
        });
    });
}

EM_JS(void, alpine_app_open_gpx_picker_and_mount, (), {
    const file_selector = document.createElement('input');
    file_selector.type = 'file';
    file_selector.accept = '.gpx';
    file_selector.addEventListener('change', event => {
        const file = event.target.files[0];
        if (!file)
            return;
        const file_reader = new FileReader();
        file_reader.addEventListener('load', load_event => {
            try {
                const data = new Uint8Array(load_event.target.result);
                // fixed file name: the picked name is not needed (the route
                // name comes from the gpx content), and sanitizing it would
                // replace the dot of the extension with an underscore
                const stream = FS.open('/tmp/planner_import/import.gpx', 'w+');
                FS.write(stream, data, 0, data.length, 0);
                FS.close(stream);
                _alpine_app_planner_import_ready();
            } catch (error) {
                console.error('Failed to load GPX file', error);
            }
        }, { once: true });
        file_reader.readAsArrayBuffer(file);
    }, { once: true });
    file_selector.click();
});

extern "C" EMSCRIPTEN_KEEPALIVE void alpine_app_routes_fs_synced()
{
    if (alpine_app_wasm_planner)
        emit alpine_app_wasm_planner->saved_routes_changed();
}

extern "C" EMSCRIPTEN_KEEPALIVE void alpine_app_planner_import_ready()
{
    if (alpine_app_wasm_planner)
        alpine_app_wasm_planner->finish_wasm_import();
}
#endif

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
// success status messages disappear on their own after this long
constexpr int k_status_expiry_ms = 3000;
// a click within this distance of a route point lands on the point (hold to
// move it) instead of appending a new one
constexpr double k_point_grab_radius = 20.0;
// a click within this distance of the planned route line asks whether a
// waypoint should be inserted at that position
constexpr double k_waypoint_insert_radius = 25.0;
// recorded tracks carry thousands of points; the per-point markers are only
// useful for hand-planned routes
constexpr int k_marker_point_limit = 50;

// saved routes live in the app data directory, one gpx file per route
QString routes_directory()
{
#ifdef __EMSCRIPTEN__
    // the idbfs mount at /idbfs is created at startup (see the fs init js
    // above) and persisted to IndexedDB on every save/delete
    return QStringLiteral("/idbfs/routes");
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/routes");
#endif
}
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

    m_status_timer.setSingleShot(true);
    m_status_timer.setInterval(k_status_expiry_ms);
    connect(&m_status_timer, &QTimer::timeout, this, [this]() {
        if (m_status == m_transient_status) {
            clear_status();
        }
    });

#ifdef __EMSCRIPTEN__
    alpine_app_wasm_planner = this;
    // pull the routes saved by a previous session out of IndexedDB; the
    // callback refreshes the saved-routes list when it lands
    alpine_app_routes_fs_init();
#endif
}

int PlannerModel::point_count() const { return int(m_plan.point_count()); }

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
    mark_plan_dirty();
    emit name_changed();
}

QString PlannerModel::description() const { return m_plan.description; }

void PlannerModel::set_description(const QString& new_description)
{
    if (m_plan.description == new_description) {
        return;
    }
    m_plan.description = new_description;
    mark_plan_dirty();
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

int PlannerModel::uphill_steepness_weight() const { return m_uphill_steepness_weight; }

void PlannerModel::set_uphill_steepness_weight(int weight)
{
    if (m_uphill_steepness_weight == weight) {
        return;
    }
    m_uphill_steepness_weight = weight;
    emit optimisation_parameters_changed();
}

int PlannerModel::uphill_penalty() const { return m_uphill_penalty; }

void PlannerModel::set_uphill_penalty(int penalty)
{
    if (m_uphill_penalty == penalty) {
        return;
    }
    m_uphill_penalty = penalty;
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
    // the chart cannot display more entries than it has pixels: very long
    // lines are resampled down (at most k_profile_max_entries variant maps
    // are ever built, so the gui never churns through tens of thousands of
    // them per update)
    constexpr int k_profile_max_entries = 300;
    QVariantList profile;
    double distance = 0.0;
    const nucleus::track::Point* previous = nullptr;
    int index = 0;
    int next_sample = 0;
    double resample_step = 0.0;
    int total_points = 0;
    for (const auto& segment : m_render_line.track) {
        total_points += int(segment.size());
    }
    if (total_points > k_profile_max_entries) {
        resample_step = double(total_points - 1) / double(k_profile_max_entries - 1);
    }
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
            if (resample_step <= 0.0 || index >= next_sample || index + 1 == total_points) {
                profile.append(QVariantMap {
                    { "x", distance },
                    { "y", point.elevation },
                    { "slope", slope_degrees },
                });
                next_sample = int(std::round(double(profile.size()) * resample_step));
            }
            if (previous) {
                const nucleus::track::Point from = { previous->latitude, previous->longitude, 0.0 };
                const nucleus::track::Point to = { point.latitude, point.longitude, 0.0 };
                distance += m_plan.haversine_distance(from, to);
            }
            previous = &point;
            index += 1;
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

bool PlannerModel::route_dirty() const { return m_route_dirty; }

void PlannerModel::mark_plan_dirty()
{
    if (m_route_dirty) {
        return;
    }
    m_route_dirty = true;
    emit route_dirty_changed();
}

void PlannerModel::set_transient_status(const QString& text)
{
    m_transient_status = text;
    m_status = text;
    emit status_changed();
    m_status_timer.start();
}

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

PlannerModel::NearestRoute PlannerModel::nearest_route_geometry(double latitude, double longitude) const
{
    NearestRoute result;
    result.point_distance = std::numeric_limits<double>::max();
    const glm::dvec2 world = nucleus::srs::lat_long_to_world({ latitude, longitude });

    int flat_index = 0;
    for (const auto& segment : m_plan.track) {
        for (const auto& point : segment) {
            const glm::dvec2 point_world = nucleus::srs::lat_long_to_world({ point.latitude, point.longitude });
            const double point_distance = glm::length(point_world - world);
            if (point_distance < result.point_distance) {
                result.point_distance = point_distance;
                result.nearest_point = flat_index;
            }
            flat_index += 1;
        }
    }
    return result;
}

int PlannerModel::nearest_render_pair(double latitude, double longitude, double* out_distance) const
{
    static const double no_distance = std::numeric_limits<double>::max();
    if (out_distance) {
        *out_distance = no_distance;
    }
    const int point_count = int(m_render_line.point_count());
    if (point_count < 2 || int(m_render_line_pair_indices.size()) < point_count) {
        return -1;
    }
    const glm::dvec2 world = nucleus::srs::lat_long_to_world({ latitude, longitude });
    double best_distance = no_distance;
    int best_pair = -1;
    for (int j = 0; j + 1 < point_count; ++j) {
        const glm::dvec2 a = nucleus::srs::lat_long_to_world(
            { m_render_line.track.front()[size_t(j)].latitude, m_render_line.track.front()[size_t(j)].longitude });
        const glm::dvec2 b = nucleus::srs::lat_long_to_world(
            { m_render_line.track.front()[size_t(j + 1)].latitude, m_render_line.track.front()[size_t(j + 1)].longitude });
        const glm::dvec2 ab = b - a;
        const double ab_length_sq = glm::dot(ab, ab);
        const double t = ab_length_sq > 0.0
            ? std::clamp(glm::dot(world - a, ab) / ab_length_sq, 0.0, 1.0)
            : 0.0;
        const double distance = glm::length(world - (a + ab * t));
        if (distance < best_distance) {
            best_distance = distance;
            best_pair = m_render_line_pair_indices[size_t(j)];
        }
    }
    if (out_distance) {
        *out_distance = best_distance == no_distance ? 0.0 : best_distance;
    }
    return best_pair;
}

void PlannerModel::insert_pairs_at(unsigned pair_index)
{
    // inserting a waypoint between waypoints pair_index and pair_index + 1
    // replaces that cached pair with two new (uncached) pairs; appending
    // after the last waypoint only adds one. emplace, not insert(pos, {}):
    // the braces bind to vector's initializer_list overload, which inserts
    // an empty list - i.e. nothing
    if (pair_index < m_optimised_segments.size()) {
        m_optimised_segments.erase(m_optimised_segments.begin() + long(pair_index));
        m_optimised_segments.emplace(m_optimised_segments.begin() + long(pair_index));
        m_optimised_segments.emplace(m_optimised_segments.begin() + long(pair_index) + 1);
    } else {
        const auto at = m_optimised_segments.begin()
            + long(std::min<size_t>(pair_index, m_optimised_segments.size()));
        m_optimised_segments.emplace(at);
    }
}

void PlannerModel::insert_waypoint(double latitude, double longitude, double altitude, int after_index)
{
    if (after_index < 0 || after_index >= int(m_plan.point_count())) {
        return;
    }
    m_plan.insert_point(unsigned(after_index) + 1, { latitude, longitude, altitude });
    insert_pairs_at(unsigned(after_index));
    if (m_optimised_segments.size() + 1 != m_plan.point_count()) {
        // the cache must hold one entry per waypoint pair; this guards the
        // insert path that silently no-op'ed once
        qWarning() << "PLANNER: optimised cache size" << m_optimised_segments.size()
                   << "does not match plan pairs" << m_plan.point_count() - 1 << "- clearing";
        m_optimised_segments.clear();
    }
    m_plan.recompute_stats();
    drop_inflight_result();
    clear_status();
    update_render_data();
    mark_plan_dirty();
    emit points_changed();
}

void PlannerModel::press_at(double latitude, double longitude)
{
    const NearestRoute near = nearest_route_geometry(latitude, longitude);
    if (near.point_distance <= k_point_grab_radius) {
        // dragging starts immediately (no hold delay); camera navigation is
        // suppressed until the pointer is released
        m_drag_point = near.nearest_point;
        m_dragging = true;
        emit route_point_dragging_changed(true);
    }
}

void PlannerModel::move_dragged(double latitude, double longitude, double altitude)
{
    if (!m_dragging || m_drag_point < 0) {
        return;
    }
    m_plan.update_point(unsigned(m_drag_point), { latitude, longitude, altitude });
    if (m_drag_point > 0) {
        clear_optimised_segment(unsigned(m_drag_point) - 1);
    }
    clear_optimised_segment(unsigned(m_drag_point));
    mark_plan_dirty();
    update_render_data();
}

void PlannerModel::release_at()
{
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    m_drag_point = -1;
    m_plan.recompute_stats();
    emit route_point_dragging_changed(false);
    emit points_changed();
}

void PlannerModel::add_point_at(double latitude, double longitude, double altitude, double screen_x, double screen_y)
{
    const NearestRoute near = nearest_route_geometry(latitude, longitude);
    if (near.point_distance <= k_point_grab_radius) {
        // clicked onto an existing point: press it to move it, a plain click
        // does nothing
        return;
    }
    // the insertion decision measures against the RENDERED line: on an
    // optimised route it follows the a* paths, which stray far from the
    // plan's straight chords - measuring against the chords would silently
    // append the clicked waypoint at the route's end instead
    double render_distance = 0;
    const int render_pair = nearest_render_pair(latitude, longitude, &render_distance);
    if (render_pair >= 0 && render_distance <= k_waypoint_insert_radius) {
        // clicked onto the rendered route line: ask whether a waypoint
        // should be created at that position, inserted into the clicked
        // waypoint pair
        emit waypoint_insertion_requested(latitude, longitude, altitude, render_pair, screen_x, screen_y);
        return;
    }

    m_plan.add_new_point({ latitude, longitude, altitude });
    m_plan.recompute_stats();
    // appending a point adds one new waypoint pair; the optimised paths of
    // all existing pairs stay valid and keep rendering. only the new pair
    // falls back to the approximate straight line until the next optimise
    drop_inflight_result();
    clear_status();
    update_render_data();
    mark_plan_dirty();
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
    mark_plan_dirty();
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
    mark_plan_dirty();
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
    mark_plan_dirty();
    emit points_changed();
}

void PlannerModel::reset()
{
    m_plan.reset();
    m_route_imported = false;
    invalidate_optimisation();
    update_render_data();
    mark_plan_dirty();
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
    // route that is rendered), otherwise the clicked waypoint plan. imported
    // routes always export their full original track: the rendered line is
    // only a light skeleton
    nucleus::track::Gpx route = (!m_optimised_segments.empty() && !m_route_imported) ? m_render_line : m_plan;
    route.creationDateTime = QDateTime::currentDateTime();
    // android file dialogs return content:// uris, which QFile can open but
    // which have no local file path
    const QString path = file_url.isLocalFile() ? file_url.toLocalFile() : file_url.toString();
    if (!route.save(path)) {
        m_status = tr("Could not write the GPX file");
        emit status_changed();
    }
}

bool PlannerModel::import_gpx(const QUrl& file_url)
{
    const QString path = file_url.isLocalFile() ? file_url.toLocalFile() : file_url.toString();
    if (path.isEmpty()) {
        qWarning() << "PlannerModel::import_gpx: not a local file" << file_url;
        m_status = tr("Could not open the selected file");
        emit status_changed();
        return false;
    }
    if (load_gpx_file(path, QFileInfo(path).completeBaseName())) {
        // imported routes are saved automatically under their name (an
        // existing route of the same name is not overwritten, the name gets
        // a counter)
        save_route(m_plan.name);
        return true;
    }
    return false;
}

bool PlannerModel::load_gpx_file(const QString& path, const QString& fallback_name)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "PlannerModel: failed to open file!" << path;
        m_status = tr("Could not open the selected file");
        emit status_changed();
        return false;
    }

    QXmlStreamReader xml_reader(&file);
    std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xml_reader);
    file.close();

    if (gpx == nullptr) {
        qWarning() << "PlannerModel: could not parse GPX file!" << path;
        m_status = tr("Could not parse the GPX file");
        emit status_changed();
        return false;
    }
    if (gpx->point_count() == 0) {
        m_status = tr("The GPX file contains no track points");
        emit status_changed();
        return false;
    }

    // the track becomes the editable plan (replaces the current one). the
    // planner edits one continuous leg and lists the points flat, so
    // multi-segment tracks are merged into a single segment: that keeps the
    // ui indices, the optimiser's pair numbering and the rendered line
    // consistent (segments of a gpx file are not connected, but a planned
    // route continues across former leg boundaries)
    m_plan = std::move(*gpx);
    m_plan.flatten_segments();
    m_route_imported = true;
    if (m_plan.name.isEmpty()) {
        m_plan.name = fallback_name;
    }
    m_plan.recompute_stats();
    invalidate_optimisation();
    update_render_data();
    emit points_changed();
    // the imported metadata replaces the current one
    emit name_changed();
    emit description_changed();

    // a route that was saved in its optimised state is optimised again on
    // open (with the current weights), so it renders as it was left
    if (m_plan.keywords.contains(QLatin1String("optimized"), Qt::CaseInsensitive)) {
        optimise_route();
    }

    // frame the whole route: the camera keeps its orientation as much as
    // possible, the route fits on screen and the camera stays above the dem
    double min_lat = 90.0, min_lon = 180.0, max_lat = -90.0, max_lon = -180.0;
    double min_alt = std::numeric_limits<double>::max();
    double max_alt = std::numeric_limits<double>::lowest();
    for (const auto& segment : m_plan.track) {
        for (const auto& point : segment) {
            min_lat = std::min(min_lat, point.latitude);
            max_lat = std::max(max_lat, point.latitude);
            min_lon = std::min(min_lon, point.longitude);
            max_lon = std::max(max_lon, point.longitude);
            min_alt = std::min(min_alt, point.elevation);
            max_alt = std::max(max_alt, point.elevation);
        }
    }
    if (min_alt <= max_alt) {
        emit route_framing_requested(min_lat, min_lon, max_lat, max_lon, min_alt, max_alt);
    }
    return true;
}

QVariantList PlannerModel::saved_routes() const
{
    QVariantList list;
    const QDir dir(routes_directory());
    for (const QFileInfo& entry : dir.entryInfoList({ QStringLiteral("*.gpx") }, QDir::Files, QDir::Name)) {
        QFile file(entry.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QXmlStreamReader xml_reader(&file);
        std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xml_reader);
        file.close();
        if (gpx == nullptr) {
            continue;
        }
        gpx->recompute_stats();
        list.append(QVariantMap {
            { "file", entry.fileName() },
            { "name", gpx->name.isEmpty() ? entry.completeBaseName() : gpx->name },
            { "points", gpx->point_count() },
            { "distance", gpx->distance },
        });
    }
    return list;
}

void PlannerModel::save_route(const QString& name)
{
    if (m_plan.point_count() == 0) {
        m_status = tr("Nothing to save");
        emit status_changed();
        return;
    }
    // the route name doubles as the file name; anything the file system
    // would not like becomes an underscore
    QString clean_name;
    for (const QChar& character : name) {
        clean_name.append(character.isLetterOrNumber() || character == ' ' || character == '-' || character == '_'
            ? character : QChar('_'));
    }
    clean_name = clean_name.trimmed();
    if (clean_name.isEmpty()) {
        clean_name = QStringLiteral("route");
    }

    // never overwrite another route: extend the name with a counter until
    // the file name is free ("Haidenspitze", "Haidenspitze 2", ...)
    const QDir dir(routes_directory());
    dir.mkpath(QStringLiteral("."));
    QString used_name = clean_name;
    int counter = 2;
    while (QFileInfo::exists(dir.filePath(used_name + QStringLiteral(".gpx")))) {
        used_name = clean_name + QLatin1Char(' ') + QString::number(counter++);
    }

    nucleus::track::Gpx route = m_plan;
    route.name = used_name;
    route.creationDateTime = QDateTime::currentDateTime();
    // tag optimised routes through the gpx keywords, so reopening them can
    // re-run the optimisation (the saved file contains only the waypoints)
    {
        constexpr QLatin1StringView k_optimized_tag("optimized");
        QStringList tags = route.keywords.split(QLatin1Char(','), Qt::SkipEmptyParts);
        tags.removeAll(QLatin1String(""));
        tags.removeAll(k_optimized_tag);
        const bool has_optimised = std::any_of(m_optimised_segments.begin(), m_optimised_segments.end(),
            [](const auto& segment) { return !segment.empty(); });
        if (has_optimised) {
            tags.prepend(k_optimized_tag);
        }
        route.keywords = tags.join(QLatin1Char(','));
    }
    if (route.save(dir.filePath(used_name + QStringLiteral(".gpx")))) {
        set_transient_status(tr("Route saved"));
#ifdef __EMSCRIPTEN__
        alpine_app_routes_fs_sync_to_idb(); // flush the idbfs mount to IndexedDB
#endif
        emit saved_routes_changed();
        if (m_route_dirty) {
            m_route_dirty = false;
            emit route_dirty_changed();
        }
    } else {
        m_status = tr("Could not save the route");
        emit status_changed();
    }
}

bool PlannerModel::load_route(const QString& file_name)
{
    if (file_name.contains('/') || file_name.contains('\\') || file_name.startsWith('.')) {
        return false;
    }
    if (!load_gpx_file(routes_directory() + QLatin1Char('/') + file_name, QFileInfo(file_name).completeBaseName())) {
        return false;
    }
    // load_gpx_file flags every load as an imported recorded track, which
    // hides the editable waypoint list; a saved route is editable when it
    // is sparse enough to have point markers - only dense recorded tracks
    // (auto-saved imports) stay flagged
    const bool dense = m_plan.point_count() > unsigned(k_marker_point_limit);
    if (m_route_imported != dense) {
        m_route_imported = dense;
        emit points_changed(); // route_imported notifies through this
    }
    if (m_route_dirty) {
        m_route_dirty = false;
        emit route_dirty_changed();
    }
    return true;
}

void PlannerModel::delete_route(const QString& file_name)
{
    if (file_name.contains('/') || file_name.contains('\\') || file_name.startsWith('.')) {
        return;
    }
    if (QFile::remove(routes_directory() + QLatin1Char('/') + file_name)) {
#ifdef __EMSCRIPTEN__
        alpine_app_routes_fs_sync_to_idb();
#endif
        emit saved_routes_changed();
    }
}

bool PlannerModel::import_route_file()
{
#ifdef __EMSCRIPTEN__
    // the qml file dialog cannot give us file contents on webassembly: the
    // browser picker is opened instead and the file is mounted into the
    // memory file system, then imported (like the track upload flow)
    QDir dir("/tmp/planner_import/");
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Files)) {
        QFile::remove(entry.absoluteFilePath()); // import only the fresh pick
    }
    dir.mkpath(QStringLiteral("."));
    alpine_app_open_gpx_picker_and_mount();
    return true;
#else
    return false;
#endif
}

#ifdef __EMSCRIPTEN__
void PlannerModel::finish_wasm_import()
{
    // the picker always mounts the picked file as import.gpx
    const QDir dir("/tmp/planner_import/");
    const QString path = dir.absoluteFilePath(QStringLiteral("import.gpx"));
    if (!QFileInfo::exists(path)) {
        qWarning() << "PlannerModel: imported file did not arrive";
        return;
    }
    if (load_gpx_file(path, QStringLiteral("import"))) {
        // imports are saved automatically, like on the desktop
        save_route(m_plan.name);
    }
    QFile::remove(path);
}
#endif

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
    weights.uphill_steepness = m_uphill_steepness_weight;
    weights.distance = m_distance_weight;
    weights.cross_slope = m_cross_slope_weight;
    weights.uphill_penalty = m_uphill_penalty / 10.0;
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
    mark_plan_dirty();
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
    mark_plan_dirty(); // an optimised route differs from what was saved
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
    // the waypoint pair each line point belongs to (parallel to the line
    // points), so clicks on the rendered line can be tied back to a plan
    // pair even when optimised paths stray from the straight chords
    std::vector<int> line_pair_indices;
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

    // recorded tracks carry thousands of points; the per-point markers are
    // only useful for hand-planned routes and would drown the map otherwise
    const bool with_markers = int(plan.point_count()) <= k_marker_point_limit;

    // accumulates steepness statistics along the rendered line. called for
    // every point appended to the line, in order; the chain restarts at each
    // plan segment (segments are not connected to each other)
    double total_abs_elevation_change = 0;
    double total_distance = 0;
    bool chain_started = false;
    nucleus::track::Point chain_previous {};
    const auto add_line_point = [&](const nucleus::track::Point& point, int pair_index) {
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
        result.line_pair_indices.push_back(pair_index);
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
        std::vector<int> line_point_pairs;
        line_points.reserve(segment.size() * 8);
        for (size_t i = 0; i < segment.size(); ++i) {
            const nucleus::track::Point& a = segment[i];
            const size_t current_pair = pair_index;
            line_points.push_back(a);
            line_point_pairs.push_back(int(current_pair));

            // point marker: ~1 m segment that the capsule shader renders as a
            // round dot sitting on the route point
            if (with_markers) {
                const double lat_offset = k_marker_length / k_metres_per_degree_latitude;
                result.markers.add_new_segment();
                result.markers.add_new_point({ a.latitude - lat_offset / 2.0, a.longitude, a.elevation });
                result.markers.add_new_point({ a.latitude + lat_offset / 2.0, a.longitude, a.elevation });
            }

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
                line_point_pairs.push_back(int(current_pair));
            }
        }

        // pin every line point to the displayed terrain: imported tracks carry
        // elevations from a different height model and would float above or
        // sink into the dem. re-sampling the interior points is idempotent
        // (they were dem-sampled above); the waypoints pick up the dem as well
        for (auto& point : line_points) {
            if (const auto alt = sampler.altitude({ point.latitude, point.longitude })) {
                point.elevation = alt.value();
            }
        }

        // moving average over the sampled elevations
        const int radius = k_elevation_smoothing_radius;
        for (size_t i = radius; i + radius < line_points.size(); ++i) {
            double sum = 0;
            for (int k = -radius; k <= radius; ++k) {
                sum += line_points[size_t(int(i) + k)].elevation;
            }
            line_points[i].elevation = sum / double(2 * radius + 1);
        }

        for (size_t j = 0; j < line_points.size(); ++j) {
            add_line_point(line_points[j], line_point_pairs[j]);
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
        m_render_line_pair_indices.clear();
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
            m_render_line_pair_indices = result.line_pair_indices;
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
    m_render_line_pair_indices = result.line_pair_indices;
    m_render_markers = result.markers;
    m_avg_steepness = result.avg_steepness;
    m_max_steepness = result.max_steepness;
    emit planned_track_changed(m_render_line, m_render_markers);
    emit render_data_changed();
#endif
}
