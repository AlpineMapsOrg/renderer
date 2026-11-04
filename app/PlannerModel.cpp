/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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
#include <QFile>
#include <QFileInfo>
#include <gl_engine/Context.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/srs.h>

namespace {
// The dem tiles are 65x65 samples; at the zoom levels the scheduler keeps
// cached when close to the terrain this resolves to roughly 20-40 m per
// sample, so sampling the route line every 20 m captures the terrain detail.
// Very long segments are coarsened automatically (see update_render_data).
constexpr double k_sample_distance = 20.0;
// upper bound of interpolation steps per segment, so that a segment across
// whole austria does not produce tens of thousands of samples
constexpr int k_max_steps_per_segment = 500;
// length of the tiny marker segment at each route point, in metres. the
// capsule shader renders the short segment as a round dot (the cap
// hemispheres dominate). horizontal, so the ribbon quad stays valid when the
// camera looks straight down (a vertical segment would degenerate there)
constexpr double k_marker_length = 1.0;
constexpr double k_metres_per_degree_latitude = 111320.0;
} // namespace

PlannerModel::PlannerModel(QObject* parent)
    : QObject(parent)
{
    auto* ctx = RenderingContext::instance();
    connect(ctx, &RenderingContext::initialised, this, [this, ctx]() {
        auto* track_manager = ctx->engine_context()->track_manager();
        connect(this, &PlannerModel::planned_track_changed, track_manager, &gl_engine::TrackManager::change_planned_track);
        connect(this, &PlannerModel::planned_track_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);

        m_data_querier = ctx->data_querier();
    });
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
    m_plan.name = new_name;
    emit points_changed();
}

QString PlannerModel::description() const { return m_plan.description; }

void PlannerModel::set_description(const QString& new_description)
{
    m_plan.description = new_description;
    emit points_changed();
}

double PlannerModel::distance() const { return m_plan.distance; }
double PlannerModel::ascent() const { return m_plan.ascent; }
double PlannerModel::descent() const { return m_plan.descent; }
double PlannerModel::avg_steepness() const { return m_avg_steepness; }
double PlannerModel::max_steepness() const { return m_max_steepness; }

void PlannerModel::add_point_at(double latitude, double longitude, double altitude)
{
    m_plan.add_new_point({ latitude, longitude, altitude });
    m_plan.recompute_stats();
    update_render_data();
    emit points_changed();
}

void PlannerModel::delete_point(int index)
{
    m_plan.delete_point(unsigned(index));
    m_plan.recompute_stats();
    update_render_data();
    emit points_changed();
}

void PlannerModel::move_point_up(int index)
{
    m_plan.move_point(unsigned(index), -1);
    update_render_data();
    emit points_changed();
}

void PlannerModel::move_point_down(int index)
{
    m_plan.move_point(unsigned(index), 1);
    update_render_data();
    emit points_changed();
}

void PlannerModel::reset()
{
    m_plan.reset();
    update_render_data();
    emit points_changed();
}

void PlannerModel::export_gpx(const QUrl& file_url)
{
    if (m_plan.point_count() == 0) {
        qDebug("PlannerModel::export_gpx: nothing to export");
        return;
    }
    m_plan.creationDateTime = QDateTime::currentDateTime();
    m_plan.save(file_url.toLocalFile());
}

void PlannerModel::import_gpx(const QUrl& file_url)
{
    const QString path = file_url.toLocalFile();
    if (path.isEmpty()) {
        qWarning() << "PlannerModel::import_gpx: not a local file" << file_url;
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "PlannerModel::import_gpx: failed to open file!" << path;
        return;
    }

    QXmlStreamReader xml_reader(&file);
    std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xml_reader);
    file.close();

    if (gpx == nullptr) {
        qWarning() << "PlannerModel::import_gpx: could not parse GPX file!" << path;
        return;
    }

    // the imported track becomes the editable plan (replaces the current one)
    m_plan = std::move(*gpx);
    if (m_plan.name.isEmpty()) {
        m_plan.name = QFileInfo(path).completeBaseName();
    }
    m_plan.recompute_stats();
    update_render_data();
    emit points_changed();

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

void PlannerModel::update_render_data()
{
    m_render_line = {};
    m_render_markers = {};
    m_avg_steepness = 0;
    m_max_steepness = 0;

    // accumulates steepness statistics along the rendered line. called for
    // every point appended to the line, in order; the chain restarts at each
    // plan segment (segments are not connected to each other)
    double total_abs_elevation_change = 0;
    double total_distance = 0;
    bool chain_started = false;
    nucleus::track::Point chain_previous {};
    const auto add_line_point = [&](const nucleus::track::Point& point) {
        if (chain_started) {
            const double dist = m_plan.haversine_distance(chain_previous, point);
            if (dist > 0) {
                const double elevation_change = std::abs(point.elevation - chain_previous.elevation);
                total_abs_elevation_change += elevation_change;
                total_distance += dist;
                m_max_steepness = std::max(m_max_steepness, glm::degrees(std::atan(elevation_change / dist)));
            }
        } else {
            chain_started = true;
        }
        chain_previous = point;
        m_render_line.add_new_point(point);
    };
    const auto restart_chain = [&chain_started]() { chain_started = false; };

    for (const auto& segment : m_plan.track) {
        restart_chain();
        for (size_t i = 0; i < segment.size(); ++i) {
            const nucleus::track::Point& a = segment[i];
            add_line_point(a);

            // point marker: ~1 m segment that the capsule shader renders as a
            // round dot sitting on the route point
            const double lat_offset = k_marker_length / k_metres_per_degree_latitude;
            m_render_markers.add_new_segment();
            m_render_markers.add_new_point({ a.latitude - lat_offset / 2.0, a.longitude, a.elevation });
            m_render_markers.add_new_point({ a.latitude + lat_offset / 2.0, a.longitude, a.elevation });

            if (i + 1 >= segment.size()) {
                continue;
            }
            const nucleus::track::Point& b = segment[i + 1];

            // interpolate between a and b, sampling the dem for the elevation
            const glm::dvec2 world_a = nucleus::srs::lat_long_to_world({ a.latitude, a.longitude });
            const glm::dvec2 world_b = nucleus::srs::lat_long_to_world({ b.latitude, b.longitude });
            const double dist = glm::distance(world_a, world_b);
            const int steps = std::min(k_max_steps_per_segment,
                std::max(1, int(std::ceil(dist / k_sample_distance))));

            for (int s = 1; s < steps; ++s) {
                const double t = double(s) / double(steps);
                const double lat = a.latitude + (b.latitude - a.latitude) * t;
                const double lon = a.longitude + (b.longitude - a.longitude) * t;

                double alt = a.elevation + (b.elevation - a.elevation) * t;
                if (m_data_querier) {
                    const std::expected<float, QString> dem_altitude = m_data_querier->get_altitude({ lat, lon });
                    if (dem_altitude.has_value()) {
                        alt = dem_altitude.value();
                    }
                }
                add_line_point({ lat, lon, alt });
            }
        }
    }

    // average steepness: atan of the mean absolute slope along the whole route
    if (total_distance > 0) {
        m_avg_steepness = glm::degrees(std::atan(total_abs_elevation_change / total_distance));
    }

    emit planned_track_changed(m_render_line, m_render_markers);
}
