/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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
#include <memory>
#include <nucleus/camera/Definition.h>
#include <nucleus/GPX.h>

#include "PolyLine.h"
#include "ShaderProgram.h"



class QOpenGLShaderProgram;

namespace gl_engine {
class ShaderProgram;

class TrackManager : public QObject {
    Q_OBJECT
public:
    explicit TrackManager(QObject* parent = nullptr);

    void init();
    void draw(const nucleus::camera::Definition& camera) const;

    void add_track(const nucleus::gpx::Gpx& gpx);
    // void remove_track();

private:
    std::unique_ptr<ShaderProgram> m_shader;
    std::vector<PolyLine> m_tracks;
};
} // namespace gl_engine
