/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Jakob Maier
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
#include <memory>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>

#include <nucleus/camera/Definition.h>
#include <nucleus/track/GPX.h>
#include <nucleus/track/Manager.h>
#include "RouteOverlay.h"

class QOpenGLShaderProgram;

namespace gl_engine {

class ShaderProgram;
class ShaderRegistry;

struct PolyLine {
    GLsizei point_count;
    std::unique_ptr<QOpenGLVertexArrayObject> vao = nullptr;
    std::unique_ptr<QOpenGLBuffer> vbo = nullptr;
    std::unique_ptr<QOpenGLTexture> texture = nullptr;
    bool planned = false;
    // route point markers (dots) get a slightly different colour than the line
    bool marker = false;
    // the user position indicator
    bool user_position = false;
    // average of the world-space track points, used to scale the width with
    // the camera distance
    glm::vec3 center = glm::vec3(0.f);
};

class TrackManager : public nucleus::track::Manager {
    Q_OBJECT
public:
    explicit TrackManager(ShaderRegistry* shader_manager, QObject* parent = nullptr);

    void draw(const nucleus::camera::Definition& camera) const;

    [[nodiscard]] ShaderProgram* shader() const;
    [[nodiscard]] const RouteOverlay& route_overlay() const { return m_route_overlay; }

public slots:
    void change_tracks(const QVector<nucleus::track::Gpx>& tracks) override;
    void change_display_width(float new_width) override;
    void change_shading_style(unsigned int new_style) override;

    // the currently planned route of the gpx editor, rendered in a distinct
    // colour. the line is densely interpolated on the dem, markers are short
    // vertical segments at the planned points
    void change_planned_track(const nucleus::track::Gpx& line, const nucleus::track::Gpx& markers);
    void clear_planned_track();

    // position indicator for the user's current gnss position (a single dot)
    void change_user_position(const nucleus::track::Gpx& marker);
    void clear_user_position();

    // hides/shows the planned route *line* (the point markers stay)
    void change_planned_line_visible(bool visible);

    // while the route preview flies, the route is painted onto the terrain
    // texture (no parallax) instead of being drawn as 3d capsules
    void set_route_overlay_active(bool active);

protected:
    void add_track(const nucleus::track::Gpx& gpx, bool planned = false, bool markers = false);

private:
    unsigned int m_shading_method = 0U;
    float m_display_width = 7.0f;
    // the planned route line is rendered with a width that scales with the
    // camera distance, so that it covers roughly this many pixels on screen
    // (clamped to a min/max world width)
    float m_planned_target_pixel_width = 5.0f;
    float m_planned_marker_pixel_width = 7.0f;
    float m_planned_min_world_width = 2.0f;
    float m_planned_max_world_width = 150.0f;
    glm::vec3 m_planned_marker_color = glm::vec3(0.0f, 0.45f, 0.9f);
    glm::vec3 m_user_position_color = glm::vec3(0.95f, 0.15f, 0.55f);
    std::shared_ptr<ShaderProgram> m_shader;
    float m_max_speed = 0.0f;
    float m_max_vertical_speed = 0.0f;
    size_t m_total_point_count = 0;
    std::vector<PolyLine> m_tracks;
    std::vector<PolyLine> m_planned_tracks;
    std::vector<PolyLine> m_user_position;
    bool m_planned_line_visible = true;
    RouteOverlay m_route_overlay;
};
} // namespace gl_engine
