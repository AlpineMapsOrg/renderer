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
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <glm/glm.hpp>
#include <memory>

#include <nucleus/planner/RouteOptimizer.h>
#include <nucleus/track/GPX.h>

namespace nucleus {
class DataQuerier;
}

// holds the route planned in the gpx editor, exposes it to QML and renders it
// via the track manager. Route points are WGS84 lat/long/altitude; the line
// between two points is interpolated on the dem via the DataQuerier.
class PlannerModel : public QObject {
    Q_OBJECT
    // list of QVariantMaps: index, latitude, longitude, elevation
    Q_PROPERTY(QVariantList points READ points NOTIFY points_changed)
    Q_PROPERTY(QString name READ name WRITE set_name NOTIFY name_changed)
    Q_PROPERTY(QString description READ description WRITE set_description NOTIFY description_changed)
    Q_PROPERTY(double distance READ distance NOTIFY points_changed)
    Q_PROPERTY(double ascent READ ascent NOTIFY points_changed)
    Q_PROPERTY(double descent READ descent NOTIFY points_changed)
    // average / maximum steepness of the planned route, in degrees, computed
    // from the dem-interpolated line (asynchronously, see render_data_changed)
    Q_PROPERTY(double avg_steepness READ avg_steepness NOTIFY render_data_changed)
    Q_PROPERTY(double max_steepness READ max_steepness NOTIFY render_data_changed)
    // least-cost path optimisation (steep ski descent planning)
    Q_PROPERTY(int steepness_weight READ steepness_weight WRITE set_steepness_weight NOTIFY optimisation_parameters_changed)
    Q_PROPERTY(int uphill_steepness_weight READ uphill_steepness_weight WRITE set_uphill_steepness_weight NOTIFY optimisation_parameters_changed)
    Q_PROPERTY(int uphill_penalty READ uphill_penalty WRITE set_uphill_penalty NOTIFY optimisation_parameters_changed)
    Q_PROPERTY(int distance_weight READ distance_weight WRITE set_distance_weight NOTIFY optimisation_parameters_changed)
    Q_PROPERTY(int cross_slope_weight READ cross_slope_weight WRITE set_cross_slope_weight NOTIFY optimisation_parameters_changed)
    // search margin around the route, in percent of the route extent
    Q_PROPERTY(int search_radius READ search_radius WRITE set_search_radius NOTIFY optimisation_parameters_changed)
    Q_PROPERTY(bool optimisation_running READ optimisation_running NOTIFY optimisation_running_changed)
    // one-line status of the editor's long-running or failing operations
    // (optimisation, gpx import/export/save); empty = nothing to report.
    // success messages disappear automatically
    Q_PROPERTY(QString status READ status NOTIFY status_changed)
    // the routes saved in the app data directory: entries are
    // { file, name, points, distance }
    Q_PROPERTY(QVariantList saved_routes READ saved_routes NOTIFY saved_routes_changed)
    // number of waypoints in the plan (cheap: use this instead of points.length)
    Q_PROPERTY(int point_count READ point_count NOTIFY points_changed)
    // true while the current plan comes from an imported gpx: recorded
    // tracks render without point markers and the editor hides the point
    // list (thousands of entries would churn the gui)
    Q_PROPERTY(bool route_imported READ route_imported NOTIFY points_changed)
    // true when the plan changed since the last save (or was never saved)
    Q_PROPERTY(bool route_dirty READ route_dirty NOTIFY route_dirty_changed)
    // hides the rendered route *line* between the points (markers stay)
    Q_PROPERTY(bool route_line_visible READ route_line_visible WRITE set_route_line_visible NOTIFY route_line_visible_changed)
    // dense elevation profile of the rendered route line: entries are
    // { x: metres along the route, y: elevation (m), slope: degrees }
    // (asynchronously computed, see render_data_changed)
    Q_PROPERTY(QVariantList elevation_profile READ elevation_profile NOTIFY render_data_changed)
public:
    explicit PlannerModel(QObject* parent = nullptr);

    QVariantList points() const;
    int point_count() const;
    QString name() const;
    void set_name(const QString& new_name);
    QString description() const;
    void set_description(const QString& new_description);
    double distance() const;
    double ascent() const;
    double descent() const;
    double avg_steepness() const;
    double max_steepness() const;
    int steepness_weight() const;
    void set_steepness_weight(int weight);
    int uphill_steepness_weight() const;
    void set_uphill_steepness_weight(int weight);
    int uphill_penalty() const;
    void set_uphill_penalty(int penalty);
    int distance_weight() const;
    void set_distance_weight(int weight);
    int cross_slope_weight() const;
    void set_cross_slope_weight(int weight);
    int search_radius() const;
    void set_search_radius(int percent);
    bool optimisation_running() const;
    QString status() const;
    bool route_dirty() const;
    bool route_line_visible() const;
    void set_route_line_visible(bool visible);
    QVariantList elevation_profile() const;
    // the routes saved in the app data directory: entries are
    // { file, name, points, distance }
    QVariantList saved_routes() const;

public slots:
    // geo coordinates in degrees/metres, as reported by the camera depth readback
    void add_point_at(double latitude, double longitude, double altitude, double screen_x = 0.0, double screen_y = 0.0);
    void delete_point(int index);
    void move_point_up(int index);
    void move_point_down(int index);
    void reset();
    void export_gpx(const QUrl& file_url);
    // loads the given gpx file into the planner (replaces the current plan);
    // false when the file could not be read or parsed
    bool import_gpx(const QUrl& file_url);
    // saves the current plan as a gpx file in the app data directory, under
    // the given name; an existing route of the same name is replaced
    void save_route(const QString& name);
    // loads a saved route (file name as listed in saved_routes) into the
    // planner; false when the file could not be read
    bool load_route(const QString& file_name);
    // removes a saved route (file name as listed in saved_routes)
    void delete_route(const QString& file_name);
    // gesture callbacks from the map (see TerrainRendererItem): a press near
    // a route point arms the hold-to-drag, dragging moves the armed point,
    // releasing ends the drag
    void press_at(double latitude, double longitude);
    void move_dragged(double latitude, double longitude, double altitude);
    void release_at();
    // inserts a waypoint into the route after the given flat waypoint index
    void insert_waypoint(double latitude, double longitude, double altitude, int after_index);
    // renders a position indicator at the user's current gnss position.
    // the altitude is only a fallback, the dem altitude is preferred
    void set_user_position(double latitude, double longitude, double altitude);
    void clear_user_position();
    // fetches high-resolution dem tiles for the planning area and replaces the
    // route between the waypoints with the least-cost descent path
    void optimise_route();
    // removes the last route point (undo for the minimised bar button)
    void undo_last_point();
    // converts the currently rendered route line into world coordinates and
    // stores it for the camera fly-through preview (map.start_route_preview)
    Q_INVOKABLE void fill_preview_route();

signals:
    void points_changed();
    // the plan metadata changed (ui text edit, import or reset); kept apart
    // from points_changed so text edits do not churn the point list
    void name_changed();
    void description_changed();
    // the dem-interpolated render line, the point markers and the
    // steepness/profile values changed; computed asynchronously after every
    // plan edit, so it arrives shortly after points_changed
    void render_data_changed();
    void planned_track_changed(const nucleus::track::Gpx& line, const nucleus::track::Gpx& markers);
    // emitted after a gpx import, with the lat/long of the first route point,
    // so the ui can move the camera to the imported track
    void track_imported(const QPointF& position);
    // a click landed close to the planned route: the ui asks whether a
    // waypoint should be created at that position (inserted into the route
    // after the given flat waypoint index)
    void waypoint_insertion_requested(double latitude, double longitude, double altitude, int after_index, double screen_x, double screen_y);
    // while a route point is dragged, the map must not receive camera
    // navigation events
    void route_point_dragging_changed(bool dragging);
    void user_position_changed(const nucleus::track::Gpx& marker);
    void user_position_cleared();
    void optimisation_running_changed();
    void status_changed();
    void route_dirty_changed();
    void saved_routes_changed();
    void optimisation_parameters_changed();
    void route_line_visible_changed(bool visible);

private slots:
    void optimisation_finished(const nucleus::planner::OptimisationResult& result);

private:
    void update_render_data();
    void invalidate_optimisation();
    // drops the cached optimised path of the given waypoint pair (an empty
    // entry renders as the approximate straight line until the next
    // optimise). waypoint edits must keep the vector aligned with the pair
    // numbering of update_render_data, so they clear entries instead of
    // erasing them
    void clear_optimised_segment(unsigned pair_index);
    void erase_optimised_segment(unsigned pair_index);
    // bump the generation so an in-flight optimisation result is discarded
    void drop_inflight_result();
    void clear_status();
    // shows a status message that disappears on its own after a few seconds
    void set_transient_status(const QString& text);
    // flags the plan as changed since the last save (enables the save button)
    void mark_plan_dirty();
    // parses a gpx file and makes it the current plan; false (with a status
    // message) on failure
    bool load_gpx_file(const QString& path, const QString& fallback_name);
    // true while the current plan comes from an imported gpx file
    bool route_imported() const { return m_route_imported; }
    // distance (metres) from a position to the nearest route point (with its
    // flat index), used for the point grab
    struct NearestRoute {
        double point_distance = 0;
        int nearest_point = -1;
    };
    NearestRoute nearest_route_geometry(double latitude, double longitude) const;
    // pair index of the rendered route line segment nearest to the position
    // (the rendered line follows the optimised paths, which stray from the
    // plan's straight chords), with its distance in metres; -1 when there is
    // no rendered line
    int nearest_render_pair(double latitude, double longitude, double* out_distance) const;
    // replaces the cached optimised path of the waypoint pair at the given
    // index with two empty entries (a new waypoint splits the pair)
    void insert_pairs_at(unsigned pair_index);
    // rebuilds the user position dot from the stored coordinates, re-sampling
    // the dem altitude from the currently cached tiles (connected to the
    // refresh timer, so the dot follows tile refinement)
    void place_user_position();
    // flattened waypoint list (lat, lon) of the whole plan
    std::vector<glm::dvec2> waypoint_sequence() const;

    // the planned route: one continuous leg (imports are flattened into a
    // single segment), so a ui point index is a flat waypoint index and
    // matches the pair numbering of update_render_data
    nucleus::track::Gpx m_plan;
    // dense, dem-interpolated version of m_plan for rendering
    nucleus::track::Gpx m_render_line;
    // one short flat segment per route point, for rendering point markers
    nucleus::track::Gpx m_render_markers;
    double m_avg_steepness = 0;
    double m_max_steepness = 0;
    std::shared_ptr<nucleus::DataQuerier> m_data_querier;

    // the user position dot: the last gnss coordinates, re-placed once per
    // interval so the dot keeps following the displayed terrain
    double m_user_position_latitude = 0;
    double m_user_position_longitude = 0;
    double m_user_position_altitude = 0;
    QTimer m_user_position_timer;

    // drag state of the route points: a press near a point immediately
    // starts dragging it, releasing ends the drag (camera navigation is
    // suppressed while it runs)
    int m_drag_point = -1;
    bool m_dragging = false;
    // the waypoint pair each rendered line point belongs to (parallel to the
    // rendered line points), for placing inserted waypoints on optimised
    // routes
    std::vector<int> m_render_line_pair_indices;

    // imported (recorded) routes render as a light skeleton instead of
    // point-by-point
    bool m_route_imported = false;

    // optimised least-cost descent path per consecutive waypoint pair, kept
    // aligned with the pair numbering of update_render_data after every
    // waypoint edit (empty entries = approximate straight interpolation)
    std::vector<std::vector<nucleus::track::Point>> m_optimised_segments;
    int m_steepness_weight = 60;
    int m_uphill_steepness_weight = 60;
    int m_uphill_penalty = 50;
    int m_distance_weight = 40;
    int m_cross_slope_weight = 50;
    int m_search_radius = 20; // percent of the route extent
    bool m_optimisation_running = false;
    QString m_status;
    QString m_transient_status; // the status text the expiry timer may clear
    bool m_route_dirty = false;
    QTimer m_status_timer;
    bool m_route_line_visible = true;
    unsigned m_optimisation_generation = 0;
    // discards results of superseded render-data computations (async, one
    // generation per update_render_data call)
    unsigned m_render_data_generation = 0;
};
