#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include "nucleus/camera/Definition.h"
#include "nucleus/tile_scheduler/DrawListGenerator.h"
#include "UniformBuffer.h"

#define SHADOWMAP_WIDTH 2048
#define SHADOWMAP_HEIGHT 2048
#define SHADOW_CASCADES 4           // HAS TO BE THE SAME AS IN shadow_config.glsl!!
#define SHADOW_CASCADES_ALIGNED 8   // either 4,8,12,16,...

class QOpenGLTexture;
class QOpenGLExtraFunctions;


namespace gl_engine {

class Framebuffer;
class ShaderProgram;
class TileManager;
class uboSharedConfig;
class uboShadowConfig;

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
