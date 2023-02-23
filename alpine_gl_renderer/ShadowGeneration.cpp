#include "ShadowGeneration.h"
#include "nucleus/camera/Definition.h"
#include "qopenglextrafunctions.h"
#include "qopenglfunctions.h"

#include <QFile>
#include <QOpenGLContext>
#include <QPainter>

const float ShadowGeneration::bg[] = {0.1f, 0.1f, 0.1f, 1.f};
const float ShadowGeneration::one[] = {1.f, 1.f, 1.f, 1.f};

/* bias Matrix for the Shadow Mapping */
const glm::mat4 ShadowGeneration::biasMatrix = glm::mat4(
            0.5, 0.0, 0.0, 0.0,
            0.0, 0.5, 0.0, 0.0,
            0.0, 0.0, 0.5, 0.0,
            0.5, 0.5, 0.5, 1.0);

void set_texture_parameters(const QOpenGLContext& context, GLuint tex_handle) {
    context.functions()->glBindTexture(GL_TEXTURE_2D, tex_handle);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
}

struct ComputeShaderResults {
    GLuint in_cnt;
    GLuint out_cnt;
    GLint in_brightness;
    GLint out_brightness;
};

ShadowGeneration::ShadowGeneration(QOpenGLContext& context_in, GLTileManager& tile_manager_in, GLuint tile_radius)
    : m_img_fb(QSize(256, 256), QOpenGLFramebufferObject::Attachment::Depth, GL_TEXTURE_2D, GL_RGBA8)
    , m_shadow_fb(QSize(256 * (tile_radius * 2 + 1), 256 * tile_radius * 2 + 1), QOpenGLFramebufferObject::Attachment::Depth, GL_TEXTURE_2D, GL_RGBA32F)
    , context(context_in)
    , tile_manager(tile_manager_in)
{
    m_ortho_program = std::make_unique<ShaderProgram>(
        ShaderProgram::Files({"gl_shaders/shadow.vert"}),
        ShaderProgram::Files({"gl_shaders/shadow.frag"}),
                "#version 440 core");

    m_sun_program = std::make_unique<ShaderProgram>(
                ShaderProgram::Files({"gl_shaders/sun.vert"}),
                ShaderProgram::Files({"gl_shaders/sun.frag"}),
                "#version 440 core");

    m_texture_program = std::make_unique<ShaderProgram>(
                ShaderProgram::Files({"gl_shaders/render_texture.vert"}),
                ShaderProgram::Files({"gl_shaders/render_texture.frag"}),
                "#version 440 core");


    QString prefix(ALP_RESOURCES_PREFIX);

    m_compute_program.addShaderFromSourceFile(QOpenGLShader::Compute, prefix + "gl_shaders/comparison.comp");
    {
        const auto link_success = m_compute_program.link();
        assert(link_success);
    }

    context.functions()->glGenTextures(1, &heatmap);
    set_texture_parameters(context, heatmap);

    context.functions()->glBindTexture(GL_TEXTURE_2D, heatmap);
    context.extraFunctions()->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);

    context.extraFunctions()->glGenVertexArrays(1, &vao);

    context.functions()->glGenBuffers(1, &compute_buffer);
    context.functions()->glBindBuffer(GL_SHADER_STORAGE_BUFFER, compute_buffer);
    context.functions()->glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ComputeShaderResults), nullptr, GL_DYNAMIC_DRAW);
}


QImage ShadowGeneration::render_sunview_shadow_ortho_tripple(const camera::Definition& camera, const camera::Definition& sun, const QImage& orthofoto, bool with_metric_heatmap) {
    QPixmap pixmap((with_metric_heatmap ? 4 : 3) * 256, 256);
    QPainter painter(&pixmap);

    qDebug() << "generate tripple start --";

    GLuint shadow_map_handle = generate_shadow_map(sun);

    painter.drawImage(0, 0, render_image(sun, sun, shadow_map_handle, 1));
    painter.drawImage(256, 0, render_image(camera, sun, shadow_map_handle, 0));
    painter.drawImage(512, 0, orthofoto);

    if (with_metric_heatmap) {
        GLenum error;

        QOpenGLTexture ortho(orthofoto, QOpenGLTexture::MipMapGeneration::DontGenerateMipMaps);
        calculate_shadow_metric(camera, sun, ortho.textureId(), heatmap);

        while((error = context.functions()->glGetError()) != GL_NONE) {
            qDebug() << "after metric: " << error;
        }

        set_texture_parameters(context, heatmap);

        while((error = context.functions()->glGetError()) != GL_NONE) {
            qDebug() << "after set texture parameters: " << error;
        }

        context.functions()->glActiveTexture(GL_TEXTURE0);
        context.functions()->glBindTexture(GL_TEXTURE_2D, heatmap);

        m_img_fb.bind();

        float zero[] = {0, 0, 0, 0};

        context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, zero);
        context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);

        while((error = context.functions()->glGetError()) != GL_NONE) {
            qDebug() << "after clear buffer: " << error;
        }

        context.extraFunctions()->glBindVertexArray(vao);
        m_texture_program->bind();
        context.functions()->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        context.extraFunctions()->glBindVertexArray(0);

        while((error = context.functions()->glGetError()) != GL_NONE) {
            qDebug() << "after draw arrays: " << error;
        }

        painter.drawImage(768, 0, m_img_fb.toImage());
    }

    return pixmap.toImage();
}



ShadowGeneration::~ShadowGeneration() {
    context.functions()->glDeleteTextures(1, &heatmap);
    context.functions()->glDeleteBuffers(1, &compute_buffer);
    context.extraFunctions()->glDeleteVertexArrays(1, &vao);
}

float ShadowGeneration::calculate_shadow_metric(const camera::Definition& camera, const camera::Definition& sun, GLuint orthofoto_handle, GLuint img_handle) {

    GLenum error;
    GLuint shadow_map_handle = generate_shadow_map(sun);

    error = context.functions()->glGetError();
    if (error != GL_NONE) {
        qDebug() << "after generate_shadow_map" << error;
    }

    run_ortho_program(camera, sun, shadow_map_handle, 2);
    GLuint in_out_texture_handle = m_img_fb.texture();
    set_texture_parameters(context, in_out_texture_handle);

    context.functions()->glBindBuffer(GL_SHADER_STORAGE_BUFFER, compute_buffer);
    ComputeShaderResults *buffer = (ComputeShaderResults*)context.extraFunctions()->glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(ComputeShaderResults), GL_MAP_WRITE_BIT);
    memset(buffer, 0, sizeof(ComputeShaderResults));
    context.extraFunctions()->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    error = context.functions()->glGetError();
    if (error != GL_NONE) {
        qDebug() << "error after setting up buffer" << error;
    }

    context.extraFunctions()->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compute_buffer);
    context.extraFunctions()->glBindImageTexture(0, orthofoto_handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    context.extraFunctions()->glBindImageTexture(1, in_out_texture_handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

    if (img_handle > 0) {
        context.extraFunctions()->glBindImageTexture(2, img_handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    }

    error = context.functions()->glGetError();
    if (error != GL_NONE) {
        qDebug() << "error after binding images" << error;
    }

    m_compute_program.bind();

    context.extraFunctions()->glDispatchCompute(256, 256, 1);

    error = context.functions()->glGetError();
    if (error != GL_NONE) {
        qDebug() << "dispach Compute error " << error;
    }


    //-----------------------------
    //here the results of the compute shader are evaluated
    ComputeShaderResults *results = (ComputeShaderResults*)context.extraFunctions()->glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(ComputeShaderResults), GL_MAP_READ_BIT);
 //  float in_ratio = results->in_cnt != 0 ? results->in_brightness / static_cast<float>(results->in_cnt) : 0;
 //  float out_ratio = results->out_cnt != 0 ? results->out_brightness / static_cast<float>(results->out_cnt) : 0;

   //in_ratio = 256 * in_ratio / 65536;
   //out_ratio = 256 * out_ratio / 65536;

   // qDebug() << "--------------------------------------";
   /* qDebug() << "in_cnt = " << results->in_cnt << " - out_cnt = " << results->out_cnt;
    qDebug() << "in_brightness = " << results->in_brightness << " - out_brightness = " << results->out_brightness;
    qDebug() << "in_ratio = " << in_ratio << " - out_ratio = " << out_ratio;
    qDebug() << "metric = " << (in_ratio - out_ratio);
*/
   // qDebug() << "brightness = " << results->in_brightness << " - gradient = " << results->out_brightness;

    float metric = static_cast<float>(results->in_brightness) / 65536 + static_cast<float>(results->out_brightness) / 65536;
    context.extraFunctions()->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  //  float metric = in_ratio - out_ratio;
   // qDebug() << "metric = " << (in_ratio - out_ratio);
    //-----------------------------

    return metric;
}

GLuint ShadowGeneration::generate_shadow_map(const camera::Definition& sun) {
    context.functions()->glViewport(0, 0, m_shadow_fb.size().width(), m_shadow_fb.size().height());

    m_shadow_fb.bind();
    context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, one);
    context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);
    m_sun_program->bind();
    tile_manager.draw(m_sun_program.get(), sun);

    GLuint shadow_map_handle = m_shadow_fb.texture();

    set_texture_parameters(context, shadow_map_handle);

    return shadow_map_handle;
}

void ShadowGeneration::run_ortho_program(const camera::Definition& camera, const camera::Definition& sun, GLuint shadow_map_handle, int mode) {
    context.functions()->glViewport(0, 0, m_img_fb.size().width(), m_img_fb.size().height());
    m_img_fb.bind();

    context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, bg);
    context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);

    context.functions()->glActiveTexture(GL_TEXTURE1);
    context.functions()->glBindTexture(GL_TEXTURE_2D, shadow_map_handle);
    context.functions()->glActiveTexture(GL_TEXTURE0);

    m_ortho_program->bind();
    m_ortho_program->set_uniform("out_mode", mode);
    m_ortho_program->set_uniform("sun_position", glm::vec3(sun.position()));
    m_ortho_program->set_uniform("shadow_matrix", biasMatrix * sun.localViewProjectionMatrix(sun.position()));

    tile_manager.draw(m_ortho_program.get(), camera);
}

QImage ShadowGeneration::render_image(const camera::Definition& camera, const camera::Definition& sun, GLuint shadow_map_handle, int mode) {
    run_ortho_program(camera, sun, shadow_map_handle, mode);
    return m_img_fb.toImage();
}
