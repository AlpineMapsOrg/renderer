/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2023 alpinemaps.org
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

#include <glm/glm.hpp>
#include <vector>
#include <QOpenGLContext>
class QOpenGLExtraFunctions;

namespace gl_engine {

class ShaderProgram;

// Generic UBO-Class for custom structs (need to be specified in UniformBuffer.cpp)
template <typename T> class UniformBuffer
{
public:

    // Generate GPU Buffer and bind to location
    void init();

    // Set Uniform Block Index (necessary because old opengl...)
    void bind_to_shader(ShaderProgram* shader);
    void bind_to_shader(std::vector<ShaderProgram*> shaders);

    // Refills the GPU Buffer
    void update_gpu_data();

    // Contains the buffer data
    T data;

    // Returns GPU-Binding Point / location
    GLuint get_buffer_location();

    /// \brief UniformBuffer Creates the template for a Uniform Buffer Object
    /// \param location The binding point/location this buffer should be bind to
    /// \param name The name of the buffer struct inside the GLSL shader
    UniformBuffer(GLuint location, const std::string& name);

    // deletes the GPU Buffer
    ~UniformBuffer();

private:

    std::string m_name;  // name of the uniform in the shader
    GLuint m_location;          // the binding point/location
    GLuint m_id;                // the gpu buffer ID

    QOpenGLExtraFunctions* m_f;

};

}

