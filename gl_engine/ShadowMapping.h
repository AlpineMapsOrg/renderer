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

#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include "nucleus/camera/Definition.h"
#include "nucleus/tile_scheduler/DrawListGenerator.h"
#include "UniformBuffer.h"

#define SHADOWMAP_WIDTH 4096
#define SHADOWMAP_HEIGHT 4096
#define SHADOW_CASCADES 4           // HAS TO BE THE SAME AS IN shadow_config.glsl!!

class QOpenGLTexture;
class QOpenGLExtraFunctions;


namespace gl_engine {

class Framebuffer;
class ShaderProgram;
class TileManager;
struct uboSharedConfig;
struct uboShadowConfig;

class ShadowMapping
{
public:

    ShadowMapping(std::shared_ptr<ShaderProgram> program,
                  std::shared_ptr<UniformBuffer<uboShadowConfig>> shadow_config,
                  std::shared_ptr<UniformBuffer<uboSharedConfig>> shared_config);

    ~ShadowMapping();

    void draw(
        TileManager* tile_manager,
        const nucleus::tile_scheduler::DrawListGenerator::TileSet draw_tileset,
        const nucleus::camera::Definition& camera);

    void bind_shadow_maps(ShaderProgram* program, unsigned int start_location);

private:

    std::shared_ptr<ShaderProgram> m_shadow_program;
    std::vector<std::unique_ptr<Framebuffer>> m_shadowmapbuffer;
    std::shared_ptr<UniformBuffer<uboShadowConfig>> m_shadow_config;
    std::shared_ptr<UniformBuffer<uboSharedConfig>> m_shared_config;
    QOpenGLExtraFunctions *m_f;

    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview);
    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, const nucleus::camera::Definition& camera, const glm::vec3& light_dir);
    std::vector<glm::mat4> getLightSpaceMatrices(const nucleus::camera::Definition& camera, const glm::vec3& light_dir);

};

}
