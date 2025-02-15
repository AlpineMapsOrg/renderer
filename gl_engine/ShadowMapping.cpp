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
#include "ShadowMapping.h"

#include "Framebuffer.h"
#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "TileGeometry.h"
#include "UniformBufferObjects.h"
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <cmath>
#include <glm/gtx/transform.hpp>

namespace gl_engine {

ShadowMapping::ShadowMapping(ShaderRegistry* shader_registry, DepthBufferClipType depth_buffer_clip_type)
    : m_depth_buffer_clip_type(depth_buffer_clip_type)
    , m_shadow_program(std::make_shared<ShaderProgram>("shadowmap.vert", "shadowmap.frag"))
{

    shader_registry->add_shader(m_shadow_program);
    m_f = QOpenGLContext::currentContext()->extraFunctions();
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        m_shadowmapbuffer.push_back(std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Float32,
            std::vector<Framebuffer::ColourFormat> {}, // no colour texture needed (=> depth only)
            glm::uvec2(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT)));
    }
}

ShadowMapping::~ShadowMapping() {

}

// broken since reverse z, projection matrix for shadowmaps probably expect -1 to 1 space, but now we have 0 to 1. otoh, hm, it works without glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)...
void ShadowMapping::draw(TileGeometry* tile_geometry,
    const nucleus::tile::IdSet& draw_tileset,
    const nucleus::camera::Definition& camera,
    std::shared_ptr<UniformBuffer<uboShadowConfig>> shadow_config,
    std::shared_ptr<UniformBuffer<uboSharedConfig>> shared_config)
{

    // NOTE: ReverseZ is not necessary for ShadowMapping since a directional light is using an orthographic projection
    // and therefore the distribution of depth is linear anyway.

    float far_plane = 100000; // Similar to camera?
    float near_plane = camera.near_plane(); // Similar to camera?
    shadow_config->data.cascade_planes[0].x = near_plane;
    shadow_config->data.cascade_planes[1].x = far_plane / 50.0f;
    shadow_config->data.cascade_planes[2].x = far_plane / 25.0f;
    shadow_config->data.cascade_planes[3].x = far_plane / 10.0f;
    shadow_config->data.cascade_planes[4].x = far_plane;
    shadow_config->data.shadowmap_size = glm::vec2(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT);

    auto qlight_dir = shared_config->data.m_sun_light_dir;
    auto light_dir = -glm::vec3(qlight_dir.x(), qlight_dir.y(), qlight_dir.z());

    for (size_t i = 0; i < SHADOW_CASCADES; ++i)
        shadow_config->data.light_space_view_proj_matrix[i] = getLightSpaceMatrix(shadow_config->data.cascade_planes[i].x, shadow_config->data.cascade_planes[i + 1].x, camera, light_dir);

    shadow_config->update_gpu_data();

    m_f->glEnable(GL_DEPTH_TEST);
    m_f->glDepthFunc(GL_LESS);
    m_f->glDisable(GL_CULL_FACE);
    m_shadow_program->bind();
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        m_shadowmapbuffer[i]->bind();
        m_f->glClearColor(0, 0, 0, 0);
        m_f->glClearDepthf(1.0f); // no reverse z
        m_f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_shadow_program->set_uniform("current_layer", i);
        tile_geometry->draw(m_shadow_program.get(), camera, draw_tileset, false, glm::dvec3(0.0));
        m_shadowmapbuffer[i]->unbind();
    }
    m_shadow_program->release();
    m_f->glEnable(GL_CULL_FACE);
}

void ShadowMapping::bind_shadow_maps(ShaderProgram* p, unsigned int start_location) {
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        std::string uname = "texin_csm";
        uname.append(std::to_string(i+1));
        unsigned int location = start_location + i;
        p->set_uniform(uname, location);
        m_shadowmapbuffer[i]->bind_depth_texture(location);
    }
}

std::vector<glm::vec4> ShadowMapping::getFrustumCornersWorldSpace(const glm::mat4& projview) const
{
    const auto inv = glm::inverse(projview);
    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
        for (unsigned int y = 0; y < 2; ++y)
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
    return frustumCorners;
}

std::vector<glm::vec4> ShadowMapping::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) const { return getFrustumCornersWorldSpace(proj * view); }

glm::mat4 ShadowMapping::getLightSpaceMatrix(const float nearPlane, const float farPlane, const nucleus::camera::Definition& camera, const glm::vec3& light_dir) const
{
    const auto fb_size = camera.viewport_size();
    const auto proj = glm::perspective(glm::radians(camera.field_of_view()), (float)fb_size.x / (float)fb_size.y, nearPlane, farPlane);
    const auto corners = getFrustumCornersWorldSpace(proj, camera.local_view_matrix());

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
        center += glm::vec3(v);
    center /= corners.size();

    const glm::dmat4 lightView = glm::lookAt(center + light_dir, center, glm::vec3(0.0f, 0.0f, 1.0f));

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    double minZ = std::numeric_limits<double>::max();
    double maxZ = std::numeric_limits<double>::lowest();
    for (const auto& v : corners)
    {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;

    if (minZ < 0) minZ *= zMult;
    else minZ /= zMult;

    if (maxZ < 0) maxZ /= zMult;
    else maxZ *= zMult;

    const glm::dmat4 lightProjection
        = (m_depth_buffer_clip_type == DepthBufferClipType::MinusOneToOne) ? glm::ortho(minX, maxX, minY, maxY, minZ, maxZ) : glm::orthoZO(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;// * glm::translate(camera.position());
}

nucleus::camera::Frustum ShadowMapping::getFrustum([[maybe_unused]] const nucleus::camera::Definition& camera)
{
    auto frustum = nucleus::camera::Frustum();
    //TODO set clipping planes and corners according to light source's view frustum
    // this would be a box covering the entire camera view frustum
    return frustum;
}

}
