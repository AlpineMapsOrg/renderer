 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "ShaderRegistry.h"

#include <QOpenGLContext>

#include "ShaderProgram.h"

 using gl_engine::ShaderProgram;
 using gl_engine::ShaderRegistry;

 ShaderRegistry::ShaderRegistry() { }

 void ShaderRegistry::add_shader(std::weak_ptr<ShaderProgram> shader) { m_program_list.push_back(shader); }

 ShaderRegistry::~ShaderRegistry() = default;

 void ShaderRegistry::reload_shaders()
 {
     std::erase_if(m_program_list, [](const auto& wp) { return wp.expired(); });
     for (const auto& pp : m_program_list) {
         auto p = pp.lock();
         if (p)
             p->reload();
     }
 }
