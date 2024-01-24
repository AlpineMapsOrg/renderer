/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>

#include "Framebuffer.h"
#include "Texture.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/map_label/MapLabelManager.h"

namespace camera {
class Definition;
}

namespace gl_engine {
class ShaderProgram;

class MapLabelManager {

public:
    explicit MapLabelManager();

    void init();
    void draw(Framebuffer* gbuffer, ShaderProgram* shader_program, const nucleus::camera::Definition& camera) const;

private:
    std::unique_ptr<Texture> m_font_texture;
    std::unique_ptr<QOpenGLTexture> m_icon_texture;

    std::unique_ptr<QOpenGLBuffer> m_vertex_buffer;
    std::unique_ptr<QOpenGLBuffer> m_index_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;
    
    nucleus::MapLabelManager m_mapLabelManager;
    unsigned long m_instance_count;
};
} // namespace gl_engine
