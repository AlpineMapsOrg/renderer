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

#include <QOpenGLTexture>
#include <glm/glm.hpp>
#include <memory>

#include <nucleus/camera/Definition.h>
#include <nucleus/track/GPX.h>

namespace gl_engine {

class ShaderProgram;

// the planned route painted onto the terrain texture: the route polyline is
// rasterised into a world-space image which the terrain shader blends into
// the albedo. being part of the drawn surface, the line has no parallax
// against the ground - unlike the 3d capsules, it cannot wiggle sideways
class RouteOverlay {
public:
    // rasterises the route (steepness coloured) into an opengl texture.
    // must be called with a current opengl context (render thread)
    void rasterise(const nucleus::track::Gpx& line);

    // binds the texture and sets the tile shader uniforms. the world origin
    // is passed camera-relative (double precision on the cpu), so the shader
    // never deals with absolute fp32 world coordinates
    void apply(ShaderProgram* tile_shader, const nucleus::camera::Definition& camera) const;

    // disables the overlay without releasing the texture
    void set_active(bool active) { m_active = active; }
    [[nodiscard]] bool active() const { return m_active; }
    [[nodiscard]] bool valid() const { return m_texture != nullptr; }

private:
    std::unique_ptr<QOpenGLTexture> m_texture;
    glm::dvec2 m_world_min = {};
    glm::dvec2 m_world_size = { 1.0, 1.0 };
    bool m_active = false;
};

} // namespace gl_engine
