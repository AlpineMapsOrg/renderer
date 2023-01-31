#ifndef SHADOWGENERATION_H
#define SHADOWGENERATION_H

#include "GLTileManager.h"
#include "ShaderProgram.h"

#include <QOpenGLFramebufferObject>



class ShadowGeneration
{
    QOpenGLFramebufferObject m_img_fb;
    QOpenGLFramebufferObject m_shadow_fb;

    std::unique_ptr<ShaderProgram> m_ortho_program;
    std::unique_ptr<ShaderProgram> m_sun_program;
    std::unique_ptr<ShaderProgram> m_texture_program;
    QOpenGLShaderProgram m_compute_program;

    GLuint heatmap;
    GLuint vao;
    GLuint compute_buffer;


    QOpenGLContext& context;
    GLTileManager& tile_manager;

    static const float bg[4];
    static const float one[4];

    static const glm::mat4 biasMatrix;

public:
    ShadowGeneration(QOpenGLContext& context, GLTileManager& tile_manager, GLuint tile_radius);
    ~ShadowGeneration();

    QImage render_sunview_shadow_ortho_tripple(const camera::Definition& camera, const camera::Definition& sun, const QImage& orthofoto, bool with_metric_heatmap = false);

    float calculate_shadow_metric(const camera::Definition& camera, const camera::Definition& sun, GLuint ortofoto_handle, GLuint img_handle = 0);

private:
    GLuint generate_shadow_map(const camera::Definition& sun);
    QImage render_image(const camera::Definition& camera, const camera::Definition& sun, GLuint shadow_map_handle, int mode);
    void run_ortho_program(const camera::Definition& camera, const camera::Definition& sun, GLuint shadow_map_handle, int mode);
};

#endif // SHADOWGENERATION_H
