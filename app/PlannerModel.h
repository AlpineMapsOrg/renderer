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

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <glm/glm.hpp>
#include <memory>

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
    Q_PROPERTY(QString name READ name WRITE set_name NOTIFY points_changed)
    Q_PROPERTY(QString description READ description WRITE set_description NOTIFY points_changed)
    Q_PROPERTY(double distance READ distance NOTIFY points_changed)
    Q_PROPERTY(double ascent READ ascent NOTIFY points_changed)
    Q_PROPERTY(double descent READ descent NOTIFY points_changed)
    // average / maximum steepness of the planned route, in degrees, computed
    // from the dem-interpolated line
    Q_PROPERTY(double avg_steepness READ avg_steepness NOTIFY points_changed)
    Q_PROPERTY(double max_steepness READ max_steepness NOTIFY points_changed)
public:
    explicit PlannerModel(QObject* parent = nullptr);

    QVariantList points() const;
    QString name() const;
    void set_name(const QString& new_name);
    QString description() const;
    void set_description(const QString& new_description);
    double distance() const;
    double ascent() const;
    double descent() const;
    double avg_steepness() const;
    double max_steepness() const;

public slots:
    // geo coordinates in degrees/metres, as reported by the camera depth readback
    void add_point_at(double latitude, double longitude, double altitude);
    void delete_point(int index);
    void move_point_up(int index);
    void move_point_down(int index);
    void reset();
    void export_gpx(const QUrl& file_url);
    // loads the given gpx file into the planner (replaces the current plan)
    void import_gpx(const QUrl& file_url);
    // renders a position indicator at the user's current gnss position.
    // the altitude is only a fallback, the dem altitude is preferred
    void set_user_position(double latitude, double longitude, double altitude);
    void clear_user_position();

signals:
    void points_changed();
    void planned_track_changed(const nucleus::track::Gpx& line, const nucleus::track::Gpx& markers);
    // emitted after a gpx import, with the lat/long of the first route point,
    // so the ui can move the camera to the imported track
    void track_imported(const QPointF& position);
    void user_position_changed(const nucleus::track::Gpx& marker);
    void user_position_cleared();

private:
    void update_render_data();

    nucleus::track::Gpx m_plan;
    // dense, dem-interpolated version of m_plan for rendering
    nucleus::track::Gpx m_render_line;
    // one short flat segment per route point, for rendering point markers
    nucleus::track::Gpx m_render_markers;
    double m_avg_steepness = 0;
    double m_max_steepness = 0;
    std::shared_ptr<nucleus::DataQuerier> m_data_querier;
};
