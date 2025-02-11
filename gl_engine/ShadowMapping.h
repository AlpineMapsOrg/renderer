/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "UniformBuffer.h"
#include "nucleus/camera/Definition.h"
#include "nucleus/tile/DrawListGenerator.h"
#include "types.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#define SHADOWMAP_WIDTH 4096
#define SHADOWMAP_HEIGHT 4096
#define SHADOW_CASCADES 4           // HAS TO BE THE SAME AS IN shadow_config.glsl!!

class QOpenGLTexture;
class QOpenGLExtraFunctions;


namespace gl_engine {

class Framebuffer;
class ShaderProgram;
class TileGeometry;
class ShaderRegistry;
struct uboSharedConfig;
struct uboShadowConfig;

class ShadowMapping
{
public:
    ShadowMapping(ShaderRegistry* shader_registry, DepthBufferClipType depth_buffer_clip_type);

    ~ShadowMapping();

    void draw(TileGeometry* tile_manager,
        const nucleus::tile::IdSet& draw_tileset,
        const nucleus::camera::Definition& camera,
        std::shared_ptr<UniformBuffer<uboShadowConfig>> shadow_config,
        std::shared_ptr<UniformBuffer<uboSharedConfig>> shared_config);

    void bind_shadow_maps(ShaderProgram* program, unsigned int start_location);
    nucleus::camera::Frustum getFrustum(const nucleus::camera::Definition& camera);

private:
    DepthBufferClipType m_depth_buffer_clip_type = DepthBufferClipType(-1);
    std::shared_ptr<ShaderProgram> m_shadow_program;
    std::vector<std::unique_ptr<Framebuffer>> m_shadowmapbuffer;
    QOpenGLExtraFunctions *m_f;

    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview) const;
    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) const;
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, const nucleus::camera::Definition& camera, const glm::vec3& light_dir) const;
    std::vector<glm::mat4> getLightSpaceMatrices(const nucleus::camera::Definition& camera, const glm::vec3& light_dir);

};

}
