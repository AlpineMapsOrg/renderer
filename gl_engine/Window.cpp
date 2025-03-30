/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Jakob Maier
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
#include "Window.h"
#include "Context.h"
#include "Framebuffer.h"
#include "SSAO.h"
#include "ShaderProgram.h"
#include "ShaderRegistry.h"
#include "ShadowMapping.h"
#include "TextureLayer.h"
#include "TileGeometry.h"
#include "TrackManager.h"
#include "UniformBufferObjects.h"
#include "helpers.h"
#include "types.h"
#include <QCoreApplication>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLVertexArrayObject>
#include <QTimer>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <nucleus/tile/drawing.h>
#include <nucleus/timing/CpuTimer.h>
#include <nucleus/timing/TimerManager.h>
#include <nucleus/utils/bit_coding.h>

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include "GpuAsyncQueryTimer.h"
#include <QOpenGLFunctions_4_5_Core>
#endif
#ifdef __ANDROID__
#include "GpuAsyncQueryTimer.h"
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif
#if defined(__ANDROID__)
#include <GLES3/gl3.h> // for GL ENUMS! DONT EXACTLY KNOW WHY I NEED THIS HERE! (on other platforms it works without)
#endif

#ifdef ALP_ENABLE_LABELS
#include "MapLabels.h"
#endif

using gl_engine::UniformBuffer;
using gl_engine::Window;
using namespace gl_engine;
using namespace nucleus::tile;

Window::Window(std::shared_ptr<Context> context)
    : m_context(context)
    , m_camera({ 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 }) // should point right at the stephansdom
{
    QTimer::singleShot(1, [this]() { emit update_requested(); });
}

Window::~Window()
{
    destroy();
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("gl_engine::~Window()");
#endif
}

void Window::initialise_gpu()
{
#if defined(ALP_ENABLE_DEV_TOOLS)
    QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
    logger->initialize();
    connect(logger, &QOpenGLDebugLogger::messageLogged, [](const QOpenGLDebugMessage& message) {
        if (message.id() == 131218)
            qDebug() << "during QOpenGLFunctions::glReadPixels" << message;
        else
            qDebug() << message;
    });
    logger->disableMessages(QList<GLuint>({ 131185 }));
    logger->disableMessages(QList<GLuint>({ 131218 }));
    logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
#endif

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    assert(f->hasOpenGLFeature(QOpenGLExtraFunctions::OpenGLFeature::MultipleRenderTargets));
    Q_UNUSED(f);

    DepthBufferClipType depth_buffer_clip_type = DepthBufferClipType::MinusOneToOne;

#if defined(__EMSCRIPTEN__)
    {
        // clang-format off
        bool success = EM_ASM_INT({
            var gl = document.querySelector('#qt-shadow-container')?.shadowRoot?.querySelector("canvas")?.getContext("webgl2");
            if (!gl) {
                console.warn("Warning: Failed to retrieve WebGL2 context while setting clip control. Clip control will not be set.");
                return 0;
            }
            const ext = gl.getExtension("EXT_clip_control");
            if (!ext) {
                console.warn("Warning: EXT_clip_control extension is not supported. As a result you might encounter z-fighting.");
                return 0;
            }
            ext.clipControlEXT(ext.LOWER_LEFT_EXT, ext.ZERO_TO_ONE_EXT);
            return 1;
        });
        // clang-format on
        if (success) {
            depth_buffer_clip_type = DepthBufferClipType::ZeroToOne;
        }
    }

#elif defined(__ANDROID__)
    if (QOpenGLContext::currentContext()->hasExtension("GL_EXT_clip_control")) {
        auto glClipControlEXT = reinterpret_cast<void (*)(GLenum, GLenum)>(QOpenGLContext::currentContext()->getProcAddress("glClipControlEXT"));

        if (glClipControlEXT) {
            glClipControlEXT(0x8CA1, 0x935F); // LOWER_LEFT_EXT, ZERO_TO_ONE_EXT
            depth_buffer_clip_type = DepthBufferClipType::ZeroToOne;
            qDebug("GL_EXT_clip_control applied: LOWER_LEFT_EXT, ZERO_TO_ONE_EXT.");
        } else {
            qWarning("glClipControlEXT function pointer could NOT be retrieved.");
        }
    } else {
        qWarning("GL_EXT_clip_control extension is NOT supported.");
    }

#else
    {
        auto f45 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_5_Core>(QOpenGLContext::currentContext());
        if (f45) {
            f45->glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // for reverse z
            depth_buffer_clip_type = DepthBufferClipType::ZeroToOne;
        }
    }
#endif

    auto* shader_registry = m_context->shader_registry();

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
            // TextureDefinition { Framebuffer::ColourFormat::R32UI }, // VertexID
        });

    m_atmospherebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector { Framebuffer::ColourFormat::RGBA8 });
    m_decoration_buffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::None, std::vector { Framebuffer::ColourFormat::RGBA8 });
    m_pickerbuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Float32, std::vector { Framebuffer::ColourFormat::RGBA32F });
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gbuffer->depth_texture()->textureId(), 0);

    m_atmosphere_shader = std::make_shared<ShaderProgram>("screen_pass.vert", "atmosphere_bg.frag");
    {
        std::vector<QString> defines;
        if (depth_buffer_clip_type == DepthBufferClipType::ZeroToOne)
            defines.push_back("#define DEPTH_BUFFER_CLIP_TYPE_ZERO_TO_ONE");
        m_compose_shader = std::make_shared<ShaderProgram>("screen_pass.vert", "compose.frag", ShaderCodeSource::FILE, defines);
    }
    m_screen_copy_shader = std::make_shared<ShaderProgram>("screen_pass.vert", "screen_copy.frag");
    shader_registry->add_shader(m_atmosphere_shader);
    shader_registry->add_shader(m_compose_shader);
    shader_registry->add_shader(m_screen_copy_shader);

    m_shadowmapping = std::make_unique<gl_engine::ShadowMapping>(shader_registry, depth_buffer_clip_type);
    m_ssao = std::make_unique<gl_engine::SSAO>(shader_registry);

    m_shared_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboSharedConfig>>(2, "shared_config");
    m_shared_config_ubo->init();
    m_shared_config_ubo->bind_to_shader(shader_registry->all());

    m_camera_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboCameraConfig>>(3, "camera_config");
    m_camera_config_ubo->init();
    m_camera_config_ubo->bind_to_shader(shader_registry->all());

    m_shadow_config_ubo = std::make_shared<gl_engine::UniformBuffer<gl_engine::uboShadowConfig>>(4, "shadow_config");
    m_shadow_config_ubo->init();
    m_shadow_config_ubo->bind_to_shader(shader_registry->all());

    { // INITIALIZE CPU AND GPU TIMER
        using namespace std;
        using nucleus::timing::CpuTimer;
        m_timer = std::make_unique<nucleus::timing::TimerManager>();

// GPU Timing Queries not supported on Web GL
#if (defined(__linux)) || defined(_WIN32) || defined(_WIN64)
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("ssao", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("atmosphere", "GPU", 240, 1.0f / 60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("tiles", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("tracks", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("shadowmap", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("compose", "GPU", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("labels", "GPU", 240, 1.0f / 60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("picker", "GPU", 240, 1.0f / 60.0f));
        m_timer->add_timer(make_shared<GpuAsyncQueryTimer>("gpu_total", "TOTAL", 240, 1.0f/60.0f));
#endif
        m_timer->add_timer(make_shared<CpuTimer>("cpu_total", "TOTAL", 240, 1.0f/60.0f));
        m_timer->add_timer(make_shared<CpuTimer>("cpu_b2b", "TOTAL", 240, 1.0f / 60.0f));
        m_timer->add_timer(make_shared<CpuTimer>("draw_list", "TOTAL", 240, 1.0f / 60.0f));
    }
}

void Window::resize_framebuffer(int width, int height)
{
    if (width == 0 || height == 0)
        return;

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    if (!f) return;
    m_gbuffer->resize({ width, height });
    {
        m_decoration_buffer->resize({ width, height });
        // we are binding the depth buffer to the m_decoration_buffer
        // m_decoration_buffer->resize automatically binds the m_decoration_buffer framebuffer
        // and glFramebufferTexture2D attaches the m_gbuffer->depth_texture() to it.
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gbuffer->depth_texture()->textureId(), 0);
    }

    m_atmospherebuffer->resize({ 1, height });
    m_ssao->resize({ width, height });
    m_pickerbuffer->resize({ width, height });
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
    m_timer->start_timer("atmosphere");
    m_atmospherebuffer->bind();
    f->glClearColor(0.0, 0.0, 0.0, 1.0);
    f->glClear(GL_COLOR_BUFFER_BIT);
    f->glDisable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_ALWAYS);
    m_atmosphere_shader->bind();
    m_screen_quad_geometry.draw();
    m_atmosphere_shader->release();
    m_timer->stop_timer("atmosphere");

    // Generate Draw-List
    // Note: Could also just be done on camera change
    m_timer->start_timer("draw_list");
    QVariantMap tile_stats;
    MapLabels::TileSet label_tile_set;
    if (m_context->map_label_manager()) {
        label_tile_set = m_context->map_label_manager()->generate_draw_list(m_camera);
        tile_stats["n_label_tiles_gpu"] = m_context->map_label_manager()->tile_count();
        tile_stats["n_label_tiles_drawn"] = unsigned(label_tile_set.size());
    }

    const auto draw_list
        = drawing::compute_bounds(drawing::limit(drawing::generate_list(m_camera, m_context->aabb_decorator(), 19), 1024u), m_context->aabb_decorator());
    const auto culled_draw_list = drawing::sort(drawing::cull(draw_list, m_camera), m_camera.position());

    tile_stats["n_geometry_tiles_gpu"] = m_context->tile_geometry()->tile_count();
    tile_stats["n_ortho_tiles_gpu"] = m_context->ortho_layer()->tile_count();
    tile_stats["n_geometry_tiles_drawn"] = unsigned(culled_draw_list.size());
    m_timer->stop_timer("draw_list");

    // DRAW SHADOWMAPS
    if (m_shared_config_ubo->data.m_csm_enabled) {
        m_timer->start_timer("shadowmap");
        m_shadowmapping->draw(m_context->tile_geometry(), draw_list, m_camera, m_shadow_config_ubo, m_shared_config_ubo);
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
        f->glClearDepthf(0.0f); // reverse z
        f->glClear(GL_DEPTH_BUFFER_BIT);
    }

    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_GREATER); // reverse z

    m_timer->start_timer("tiles");
    m_context->ortho_layer()->draw(*m_context->tile_geometry(), m_camera, culled_draw_list);
    m_timer->stop_timer("tiles");

    m_gbuffer->unbind();


    if (m_shared_config_ubo->data.m_ssao_enabled) {
        m_timer->start_timer("ssao");
        m_ssao->draw(m_gbuffer.get(), &m_screen_quad_geometry, m_camera, m_shared_config_ubo->data.m_ssao_kernel, m_shared_config_ubo->data.m_ssao_blur_kernel_size);
        m_timer->stop_timer("ssao");
    }

    {
        m_timer->start_timer("picker");
        m_pickerbuffer->bind();

        // CLEAR PICKER BUFFER
        f->glClearColor(0.0, 0.0, 0.0, 0.0);
        f->glClear(GL_COLOR_BUFFER_BIT);
        f->glClear(GL_DEPTH_BUFFER_BIT);

        // DRAW Pickbuffer
        if (m_context->map_label_manager())
            m_context->map_label_manager()->draw_picker(m_gbuffer.get(), m_camera, label_tile_set);
        m_timer->stop_timer("picker");
    }

    if (framebuffer)
        framebuffer->bind();
    else
        f->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_compose_shader->bind();
    m_compose_shader->set_uniform("texin_albedo", 0);
    m_gbuffer->bind_colour_texture(0, 0);
    m_compose_shader->set_uniform("texin_position", 1);
    m_gbuffer->bind_colour_texture(1, 1);
    m_compose_shader->set_uniform("texin_normal", 2);
    m_gbuffer->bind_colour_texture(2, 2);

    m_compose_shader->set_uniform("texin_atmosphere", 3);
    m_atmospherebuffer->bind_colour_texture(0, 3);

    m_compose_shader->set_uniform("texin_ssao", 4);
    m_ssao->bind_ssao_texture(4);

    /* texture units 5 - 8 */
    m_shadowmapping->bind_shadow_maps(m_compose_shader.get(), 5);

    m_timer->start_timer("compose");
    m_screen_quad_geometry.draw();
    m_timer->stop_timer("compose");

    m_decoration_buffer->bind();
    const GLfloat clearAlbedoColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    f->glClearBufferfv(GL_COLOR, 0, clearAlbedoColor);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_GEQUAL); // reverse z

    // DRAW LABELS
    if (m_context->map_label_manager()) {
        m_timer->start_timer("labels");
        m_context->map_label_manager()->draw(m_gbuffer.get(), m_camera, label_tile_set);
        m_timer->stop_timer("labels");
    }

    // DRAW TRACKS
    {

        if (m_context->track_manager()) {
            m_timer->start_timer("tracks");
            auto* track_shader = m_context->track_manager()->shader();
            track_shader->bind();
            track_shader->set_uniform("texin_position", 1);
            m_gbuffer->bind_colour_texture(1, 1);

            glm::vec2 size = glm::vec2(static_cast<float>(m_gbuffer->size().x), static_cast<float>(m_gbuffer->size().y));
            track_shader->set_uniform("resolution", size);

            f->glClear(GL_DEPTH_BUFFER_BIT);
            m_context->track_manager()->draw(m_camera);

            m_timer->stop_timer("tracks");
        }
    }

    if (framebuffer)
        framebuffer->bind();
    else
        f->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_screen_copy_shader->bind();
    m_decoration_buffer->bind_colour_texture(0, 0);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_screen_quad_geometry.draw();

    f->glDisable(GL_BLEND);
    f->glBlendFunc(GL_ONE, GL_ZERO);
    f->glDisable(GL_CULL_FACE);

    m_timer->stop_timer("cpu_total");
    m_timer->stop_timer("gpu_total");

#if defined(ALP_ENABLE_DEV_TOOLS) && defined(__linux__)
    // make time measurment more stable.
    glFinish();
#endif

    QList<nucleus::timing::TimerReport> new_values = m_timer->fetch_results();
    if (new_values.size() > 0) {
        emit timer_measurements_ready(new_values);
    }
    emit tile_stats_ready(tile_stats);
}

void Window::shared_config_changed(gl_engine::uboSharedConfig ubo) {
    m_shared_config_ubo->data = ubo;
    m_shared_config_ubo->update_gpu_data();
    emit update_requested();
}

void Window::reload_shader() {
    auto do_reload = [this]() {
        auto* shader_manager = m_context->shader_registry();
        shader_manager->reload_shaders();
        // NOTE: UBOs need to be reattached to the programs!
        m_shared_config_ubo->bind_to_shader(shader_manager->all());
        m_camera_config_ubo->bind_to_shader(shader_manager->all());
        m_shadow_config_ubo->bind_to_shader(shader_manager->all());
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

float Window::depth(const glm::dvec2& normalised_device_coordinates)
{
    const auto read_float = nucleus::utils::bit_coding::to_f16f16(m_gbuffer->read_colour_attachment_pixel<glm::u8vec4>(3, normalised_device_coordinates))[0];
    const auto depth = std::exp(read_float * 13.f);
    return depth;
}

void Window::pick_value(const glm::dvec2& screen_space_coordinates)
{
    const auto value
        = nucleus::utils::bit_coding::f8_4_to_u32(m_pickerbuffer->read_colour_attachment_pixel<glm::vec4>(0, m_camera.to_ndc(screen_space_coordinates)));
    emit value_picked(value);
}

glm::dvec3 Window::position(const glm::dvec2& normalised_device_coordinates)
{
    return m_camera.position() + m_camera.ray_direction(normalised_device_coordinates) * (double)depth(normalised_device_coordinates);
}

void Window::destroy()
{
    m_gbuffer.reset();
    m_screen_quad_geometry = {};
}

nucleus::camera::AbstractDepthTester* Window::depth_tester() { return this; }

nucleus::utils::ColourTexture::Format Window::ortho_tile_compression_algorithm() const { return Texture::compression_algorithm(); }
