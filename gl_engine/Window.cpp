/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Lucas Dworschak
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
#include <QCoreApplication>

#include <QDebug>
#include <QImage>
#include <QMoveEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>

#include <QOpenGLVersionFunctionsFactory>

#include "Framebuffer.h"
#include "MapLabelManager.h"
#include "SSAO.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "ShadowMapping.h"
#include "TileManager.h"
#include "Window.h"
#include "helpers.h"
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <glm/glm.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "UniformBufferObjects.h"

#include "nucleus/timing/TimerManager.h"
#include "nucleus/timing/CpuTimer.h"
#include "nucleus/utils/bit_coding.h"
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include "GpuAsyncQueryTimer.h"
#endif

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include <QOpenGLFunctions_3_3_Core> // for wireframe mode
#endif

#if defined(__ANDROID__)
#include <GLES3/gl3.h> // for GL ENUMS! DONT EXACTLY KNOW WHY I NEED THIS HERE! (on other platforms it works without)
#endif

 using gl_engine::Window;
 using gl_engine::UniformBuffer;

 Window::Window()
     : m_camera({ 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 }) // should point right at the stephansdom
 {
     m_tile_manager = std::make_unique<TileManager>();
     m_map_label_manager = std::make_shared<MapLabelManager>();
     QTimer::singleShot(1, [this]() { emit update_requested(); });
}

Window::~Window()
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("gl_engine::~Window()");
#endif
}

void Window::initialise_gpu()
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    assert(f->hasOpenGLFeature(QOpenGLExtraFunctions::OpenGLFeature::MultipleRenderTargets));
    Q_UNUSED(f);

    QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
    logger->initialize();
    connect(logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger->disableMessages(QList<GLuint>({ 131185 }));
    logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    m_shader_manager = std::make_unique<ShaderManager>();

    m_tile_manager->init();
    m_tile_manager->initilise_attribute_locations(m_shader_manager->tile_shader());
    m_screen_quad_geometry = gl_engine::helpers::create_screen_quad_geometry();
    // NOTE to position buffer: The position can not be recalculated by depth alone. (given the numerical resolution of the depth buffer and
    // our massive view spektrum). ReverseZ would be an option but isnt possible on WebGL and OpenGL ES (since their depth buffer is aligned from -1...1)
    // I implemented reverse Z at some point natively (just look for the comments "for ReverseZ" in the whole solution). Even with reverse Z the
    // reconstruction of the position inside the illumination shaders is not perfect though. Thats why we have this 4x32bit position buffer.
    // Also don't try to reconstruct the position from the discretized Encoded Depth buffer. Thats wrong for a lot of reasons and it should purely be
    // used for screen interaction on close surfaces.
    // NEXT NOTE in regards to distance: I save the distance inside the position buffer to not have to calculate it all of the time
    // by the position. IMPORTANT: I also reset it to -1 such that i know when a pixel was not processed in tile shader!!
    // ANOTHER IMPORTANT NOTE: RGB32f, RGB16f are not supported by OpenGL ES and/or WebGL
    m_gbuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Float32,
        std::vector {
            Framebuffer::ColourFormat::RGBA8, // Albedo
            Framebuffer::ColourFormat::RGBA32F, // Position WCS and distance (distance is optional, but i use it directly for a little speed improvement)
            Framebuffer::ColourFormat::RG16UI, // Octahedron Normals
            Framebuffer::ColourFormat::RGBA8, // Discretized Encoded Depth for readback IMPORTANT: IF YOU MOVE THIS YOU HAVE TO ADAPT THE GET DEPTH FUNCTION
        });

    m_atmospherebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector { Framebuffer::ColourFormat::RGBA8 });
    m_decoration_buffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector { Framebuffer::ColourFormat::RGBA8 });
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gbuffer->depth_texture()->textureId(), 0);

    m_shared_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboSharedConfig>>(0, "shared_config");
    m_shared_config_ubo->init();
    m_shared_config_ubo->bind_to_shader(m_shader_manager->all());

    m_camera_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboCameraConfig>>(1, "camera_config");
    m_camera_config_ubo->init();
    m_camera_config_ubo->bind_to_shader(m_shader_manager->all());

    m_shadow_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboShadowConfig>>(2, "shadow_config");
    m_shadow_config_ubo->init();
    m_shadow_config_ubo->bind_to_shader(m_shader_manager->all());

    m_ssao = std::make_unique<gl_engine::SSAO>(m_shader_manager->shared_ssao_program(), m_shader_manager->shared_ssao_blur_program());

    m_shadowmapping = std::make_unique<gl_engine::ShadowMapping>(m_shader_manager->shared_shadowmap_program(), m_shadow_config_ubo, m_shared_config_ubo);

    m_map_label_manager->init();

    {   // INITIALIZE CPU AND GPU TIMER
        using namespace std;
        using nucleus::timing::CpuTimer;
        m_timer = std::make_unique<nucleus::timing::TimerManager>();

// GPU Timing Queries not supported on OpenGL ES or Web GL
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("ssao", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("atmosphere", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("tiles", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("shadowmap", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("compose", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("labels", "GPU", 240, 1.0f / 60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("gpu_total", "TOTAL", 240, 1.0f/60.0f));
#endif
        m_timer->add_timer(make_shared<CpuTimer>("cpu_total", "TOTAL", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<CpuTimer>("cpu_b2b", "TOTAL", 240, 1.0f/60.0f));
    }

    emit gpu_ready_changed(true);
}

void Window::resize_framebuffer(int width, int height)
{
    if (width == 0 || height == 0)
        return;

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    if (!f) return;
    m_gbuffer->resize({ width, height });
    m_decoration_buffer->resize({ width, height });
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gbuffer->depth_texture()->textureId(), 0);

    m_atmospherebuffer->resize({ 1, height });
    m_ssao->resize({ width, height });
}

void Window::paint(QOpenGLFramebufferObject* framebuffer)
{
    m_timer->start_timer("cpu_total");
    m_timer->start_timer("gpu_total");

    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

    f->glEnable(GL_CULL_FACE);
    f->glCullFace(GL_BACK);

    // UPDATE CAMERA UNIFORM BUFFER
    // NOTE: Could also just be done on camera or viewport change!
    uboCameraConfig* cc = &m_camera_config_ubo->data;
    cc->position = glm::vec4(m_camera.position(), 1.0);
    cc->view_matrix = m_camera.local_view_matrix();
    cc->proj_matrix = m_camera.projection_matrix();
    cc->view_proj_matrix = cc->proj_matrix * cc->view_matrix;
    cc->inv_view_proj_matrix = glm::inverse(cc->view_proj_matrix);
    cc->inv_view_matrix = glm::inverse(cc->view_matrix);
    cc->inv_proj_matrix = glm::inverse(cc->proj_matrix);
    cc->viewport_size = m_camera.viewport_size();
    cc->distance_scaling_factor = m_camera.distance_scale_factor();
    m_camera_config_ubo->update_gpu_data();


    // DRAW ATMOSPHERIC BACKGROUND
    m_atmospherebuffer->bind();
    f->glClearColor(0.0, 0.0, 0.0, 1.0);
    f->glClear(GL_COLOR_BUFFER_BIT);
    f->glDisable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_ALWAYS);
    auto p = m_shader_manager->atmosphere_bg_program();
    p->bind();
    m_timer->start_timer("atmosphere");
    m_screen_quad_geometry.draw();
    m_timer->stop_timer("atmosphere");
    p->release();

    // Generate Draw-List
    // Note: Could also just be done on camera change
    m_timer->start_timer("draw_list");
    const auto tile_set = m_tile_manager->generate_tilelist(m_camera);
    m_timer->stop_timer("draw_list");

    // DRAW SHADOWMAPS
    if (m_shared_config_ubo->data.m_csm_enabled) {
        m_timer->start_timer("shadowmap");
        m_shadowmapping->draw(m_tile_manager.get(), tile_set, m_camera);
        m_timer->stop_timer("shadowmap");
    }

    // DRAW GBUFFER
    m_gbuffer->bind();

    {
        // Clear Albedo-Buffer
        const GLfloat clearAlbedoColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        f->glClearBufferfv(GL_COLOR, 0, clearAlbedoColor);
        // Clear Position-Buffer (IMPORTANT [4] to <0, such that i know by sign if fragment was processed)
        const GLfloat clearPositionColor[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
        f->glClearBufferfv(GL_COLOR, 1, clearPositionColor);
        // Clear Normals-Buffer
        const GLuint clearNormalColor[2] = { 0u, 0u };
        f->glClearBufferuiv(GL_COLOR, 2, clearNormalColor);
        // Clear Encoded-Depth Buffer
        const GLfloat clearEncDepthColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        f->glClearBufferfv(GL_COLOR, 3, clearEncDepthColor);
        // Clear Depth-Buffer
        // f->glClearDepthf(0.0f); // for reverse z
        f->glClear(GL_DEPTH_BUFFER_BIT);
    }

    f->glEnable(GL_DEPTH_TEST);
    // f->glDepthFunc(GL_GREATER); // for reverse z
    f->glDepthFunc(GL_LESS);

    m_shader_manager->tile_shader()->bind();
    m_timer->start_timer("tiles");
    auto culled_tile_set = m_tile_manager->cull(tile_set, m_camera.frustum());
    m_tile_manager->draw(m_shader_manager->tile_shader(), m_camera, culled_tile_set, true, m_camera.position());
    m_timer->stop_timer("tiles");
    m_shader_manager->tile_shader()->release();

    m_gbuffer->unbind();

    m_shader_manager->tile_shader()->release();

    if (m_shared_config_ubo->data.m_ssao_enabled) {
        m_timer->start_timer("ssao");
        m_ssao->draw(m_gbuffer.get(), &m_screen_quad_geometry, m_camera, m_shared_config_ubo->data.m_ssao_kernel, m_shared_config_ubo->data.m_ssao_blur_kernel_size);
        m_timer->stop_timer("ssao");
    }

    if (framebuffer)
        framebuffer->bind();

    p = m_shader_manager->compose_program();

    p->bind();
    p->set_uniform("texin_albedo", 0);
    m_gbuffer->bind_colour_texture(0, 0);
    p->set_uniform("texin_position", 1);
    m_gbuffer->bind_colour_texture(1, 1);
    p->set_uniform("texin_normal", 2);
    m_gbuffer->bind_colour_texture(2, 2);
    p->set_uniform("texin_atmosphere", 3);
    m_atmospherebuffer->bind_colour_texture(0, 3);
    p->set_uniform("texin_ssao", 4);
    m_ssao->bind_ssao_texture(4);

    m_shadowmapping->bind_shadow_maps(p, 5);

    m_timer->start_timer("compose");
    m_screen_quad_geometry.draw();
    m_timer->stop_timer("compose");

    // DRAW LABELS
    m_timer->start_timer("labels");
    {
        m_decoration_buffer->bind();
        const GLfloat clearAlbedoColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        f->glClearBufferfv(GL_COLOR, 0, clearAlbedoColor);
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LEQUAL);
        // f->glDepthMask(GL_FALSE);
        m_shader_manager->labels_program()->bind();
        m_map_label_manager->draw(m_gbuffer.get(), m_shader_manager->labels_program(), m_camera, culled_tile_set);
        m_shader_manager->labels_program()->release();

        if (framebuffer)
            framebuffer->bind();
        m_shader_manager->screen_copy_program()->bind();
        m_decoration_buffer->bind_colour_texture(0, 0);
        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_screen_quad_geometry.draw();
    }

    m_timer->stop_timer("labels");

    m_timer->stop_timer("cpu_total");
    m_timer->stop_timer("gpu_total");

    QList<nucleus::timing::TimerReport> new_values = m_timer->fetch_results();
    if (new_values.size() > 0) {
        emit report_measurements(new_values);
    }

}

void Window::shared_config_changed(gl_engine::uboSharedConfig ubo) {
    m_shared_config_ubo->data = ubo;
    m_shared_config_ubo->update_gpu_data();
    emit update_requested();
}

void Window::reload_shader() {
    auto do_reload = [this]() {
        m_shader_manager->reload_shaders();
        // NOTE: UBOs need to be reattached to the programs!
        m_shared_config_ubo->bind_to_shader(m_shader_manager->all());
        m_camera_config_ubo->bind_to_shader(m_shader_manager->all());
        m_shadow_config_ubo->bind_to_shader(m_shader_manager->all());
        qDebug("all shaders reloaded");
        emit update_requested();
    };
#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD
    // Reload shaders from the web and afterwards do the reload
    ShaderProgram::web_download_shader_files_and_put_in_cache(do_reload);
#else
      // Reset shader cache. The shaders will then be reload from file
    ShaderProgram::reset_shader_cache();
    do_reload();
#endif
}

void Window::updateCameraEvent()
{
    emit update_camera_requested();
}

void Window::set_permissible_screen_space_error(float new_error) { m_tile_manager->set_permissible_screen_space_error(new_error); }

void Window::set_quad_limit(unsigned int new_limit) { m_tile_manager->set_quad_limit(new_limit); }

void Window::update_camera(const nucleus::camera::Definition& new_definition)
{
    //    qDebug("void Window::update_camera(const nucleus::camera::Definition& new_definition)");
    m_camera = new_definition;
    emit update_requested();
}

void Window::update_debug_scheduler_stats(const QString& stats)
{
    m_debug_scheduler_stats = stats;
    emit update_requested();
}

void Window::update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    assert(m_tile_manager);
    m_tile_manager->update_gpu_quads(new_quads, deleted_quads);

    assert(m_map_label_manager);
    m_map_label_manager->update_gpu_quads(new_quads, deleted_quads);
}

float Window::depth(const glm::dvec2& normalised_device_coordinates)
{
    const auto read_float = nucleus::utils::bit_coding::to_f16f16(m_gbuffer->read_colour_attachment_pixel<glm::u8vec4>(3, normalised_device_coordinates))[0];
    const auto depth = std::exp(read_float * 13.f);
    return depth;
}

glm::dvec3 Window::position(const glm::dvec2& normalised_device_coordinates)
{
    return m_camera.position() + m_camera.ray_direction(normalised_device_coordinates) * (double)depth(normalised_device_coordinates);
}

void Window::deinit_gpu()
{
    emit gpu_ready_changed(false);
    m_tile_manager.reset();
    m_shader_manager.reset();
    m_gbuffer.reset();
    m_screen_quad_geometry = {};
    m_map_label_manager.reset();
}

void Window::set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator)
{
    assert(m_tile_manager);
    m_tile_manager->set_aabb_decorator(new_aabb_decorator);
}

nucleus::camera::AbstractDepthTester* Window::depth_tester() { return this; }

nucleus::utils::ColourTexture::Format Window::ortho_tile_compression_algorithm() const { return Texture::compression_algorithm(); }
