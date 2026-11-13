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

#include "RouteOverlay.h"

#include "ShaderProgram.h"
#include <QImage>
#include <QPainter>
#include <QPen>
#include <nucleus/srs.h>

#include <algorithm>
#include <cmath>

namespace gl_engine {

namespace {
// steepness legend colours (same bins as the steepness overlay)
QColor steepness_color(double slope_degrees)
{
    if (slope_degrees < 15.0) return QColor(254, 249, 249);
    if (slope_degrees < 25.0) return QColor(0, 115, 25);
    if (slope_degrees < 30.0) return QColor(51, 249, 49);
    if (slope_degrees < 35.0) return QColor(242, 228, 44);
    if (slope_degrees < 40.0) return QColor(255, 169, 45);
    if (slope_degrees < 45.0) return QColor(255, 48, 45);
    if (slope_degrees < 50.0) return QColor(255, 79, 249);
    if (slope_degrees < 55.0) return QColor(183, 69, 253);
    if (slope_degrees < 60.0) return QColor(135, 44, 253);
    return QColor(49, 49, 253);
}

constexpr int k_texture_unit = 9;
constexpr double k_line_width_metres = 8.0;
constexpr int k_max_texture_dimension = 2048;
constexpr double k_bbox_padding_metres = 60.0;
} // namespace

void RouteOverlay::rasterise(const nucleus::track::Gpx& line)
{
    // collect the world-space route points
    std::vector<glm::dvec2> points;
    std::vector<double> slopes;
    for (const auto& segment : line.track) {
        for (size_t i = 0; i < segment.size(); ++i) {
            const auto& point = segment[i];
            points.push_back(nucleus::srs::lat_long_to_world({ point.latitude, point.longitude }));
            if (i + 1 < segment.size()) {
                const auto& next = segment[i + 1];
                const double dx = glm::length(points.back() - nucleus::srs::lat_long_to_world({ next.latitude, next.longitude }));
                const double dz = std::abs(next.elevation - point.elevation);
                slopes.push_back(std::atan2(dz, dx) * 180.0 / M_PI);
            }
        }
    }
    if (points.size() < 2) {
        m_texture.reset();
        return;
    }

    glm::dvec2 min = points.front(), max = points.front();
    for (const auto& p : points) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
    min -= glm::dvec2(k_bbox_padding_metres);
    max += glm::dvec2(k_bbox_padding_metres);
    m_world_min = min;
    m_world_size = max - min;

    const double px_per_metre = std::min(double(k_max_texture_dimension) / std::max(m_world_size.x, m_world_size.y), 4.0);
    const int width = std::max(2, int(m_world_size.x * px_per_metre));
    const int height = std::max(2, int(m_world_size.y * px_per_metre));

    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double pen_width = std::max(2.0, k_line_width_metres * px_per_metre);

    // world -> image pixels; north (max y) is the top row, matching how
    // qopengltexture uploads the image
    const auto to_pixel = [&](const glm::dvec2& p) {
        return QPointF((p.x - m_world_min.x) / m_world_size.x * double(width),
            (max.y - p.y) / m_world_size.y * double(height));
    };

    size_t slope_index = 0;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        if (slope_index < slopes.size()) {
            painter.setPen(QPen(steepness_color(slopes[slope_index]), pen_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            ++slope_index;
        }
        painter.drawLine(to_pixel(points[i]), to_pixel(points[i + 1]));
    }
    painter.end();

    m_texture = std::make_unique<QOpenGLTexture>(image);
    m_texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void RouteOverlay::apply(ShaderProgram* tile_shader, const nucleus::camera::Definition& camera) const
{
    if (!valid()) {
        return;
    }
    m_texture->bind(k_texture_unit);
    tile_shader->set_uniform("texin_route", k_texture_unit);
    // camera-relative origin (computed in double precision): fp32 world
    // coordinates are too imprecise at web-mercator magnitudes
    tile_shader->set_uniform("route_params",
        glm::vec4(float(m_world_min.x - camera.position().x),
            float(m_world_min.y - camera.position().y),
            float(1.0 / m_world_size.x),
            float(1.0 / m_world_size.y)));
    tile_shader->set_uniform("route_overlay_strength", m_active ? 0.85f : 0.0f);
}

} // namespace gl_engine
