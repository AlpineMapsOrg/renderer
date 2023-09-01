#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include "helpers.h"
#include "nucleus/camera/Definition.h"

class QOpenGLTexture;
class QOpenGLExtraFunctions;

namespace gl_engine {

class Framebuffer;
class ShaderProgram;

class SSAO
{
public:

    SSAO(std::shared_ptr<ShaderProgram> program);

    // deletes the GPU Buffer
    ~SSAO();

    void draw(Framebuffer* gbuffer, helpers::ScreenQuadGeometry* geometry, const nucleus::camera::Definition& camera);

    void resize(glm::uvec2 vp_size);

    void bind_ssao_texture(unsigned int location);

private:

    std::vector<glm::vec3> m_ssao_kernel;
    std::unique_ptr<QOpenGLTexture> m_ssao_noise_texture;
    std::unique_ptr<Framebuffer> m_ssaobuffer;
    std::shared_ptr<ShaderProgram> m_ssao_program;
    QOpenGLExtraFunctions *m_f;

};

}
