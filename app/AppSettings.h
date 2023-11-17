/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "qdatetime.h"
#include <QObject>
#include <QDateTime>

#include "nucleus/utils/UrlModifier.h"

// This class contains properties which are specific for the rendering APP.
// NOTE: The idea is to unclutter the TerrainRenderItem a little bit. The two
// objects are still tightly coupled. Better options are very much welcome, but might
// require significant refactorization.
class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QDateTime datetime READ datetime WRITE set_datetime NOTIFY datetime_changed)
    Q_PROPERTY(bool gl_sundir_date_link READ gl_sundir_date_link WRITE set_gl_sundir_date_link NOTIFY gl_sundir_date_link_changed)
    Q_PROPERTY(float render_quality READ render_quality WRITE set_render_quality NOTIFY render_quality_changed)

signals:
    void datetime_changed(const QDateTime& new_datetime);
    void gl_sundir_date_link_changed(bool new_value);
    void render_quality_changed(float new_value);

public:

    // Constructs settings object and loads the properties from QSettings
    AppSettings(QObject* parent, std::shared_ptr<nucleus::utils::UrlModifier> url_modifier);
    ~AppSettings();

    const QDateTime& datetime() const { return m_datetime; }
    void set_datetime(const QDateTime& new_datetime);

    bool gl_sundir_date_link() const { return m_gl_sundir_date_link; }
    void set_gl_sundir_date_link(bool new_value);

    float render_quality() const { return m_render_quality; }
    void set_render_quality(float new_value);

private:
    // Stores date and time for the current rendering (in use for eg. Shadows)
    QDateTime m_datetime = QDateTime::currentDateTime();
    // if true the sun light direction will be evaluated based on m_datetime
    bool m_gl_sundir_date_link = true;
    // Value which defines the quality of tiles being fetched (values from [0.1-2.0] make sense)
    float m_render_quality = 0.5;

    // Parents instance of UrlModififer
    std::shared_ptr<nucleus::utils::UrlModifier> m_url_modifier;
    // Loads settings from given url_manager
    void load_from_url();


};
