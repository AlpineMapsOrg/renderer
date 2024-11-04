/*****************************************************************************
 * AlpineMaps.org
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
#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include "helpers.h"
#include "nucleus/camera/Definition.h"

#define MAX_SSAO_KERNEL_SIZE 64 // ALSO CHANGE IN ssao.frag

class QOpenGLTexture;
class QOpenGLExtraFunctions;

namespace gl_engine {

class Framebuffer;
class ShaderProgram;
class ShaderRegistry;

class SSAO
{
public:
    SSAO(ShaderRegistry* shader_registry);

    // deletes the GPU Buffer
    ~SSAO();

    void draw(Framebuffer* gbuffer, helpers::ScreenQuadGeometry* geometry,
              const nucleus::camera::Definition& camera, unsigned int kernel_size, unsigned int blur_level);

    void resize(glm::uvec2 vp_size);

    void bind_ssao_texture(unsigned int location);

private:

    std::vector<glm::vec3> m_ssao_kernel;
    std::unique_ptr<QOpenGLTexture> m_ssao_noise_texture;
    std::unique_ptr<Framebuffer> m_ssaobuffer;
    std::unique_ptr<Framebuffer> m_ssao_blurbuffer;
    std::shared_ptr<ShaderProgram> m_ssao_program;
    std::shared_ptr<ShaderProgram> m_ssao_blur_program;
    QOpenGLExtraFunctions *m_f;

    void recreate_kernel(unsigned int size = 64);

};

}
