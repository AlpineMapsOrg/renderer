/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <array>

#include <QDebug>
#include <QImage>
#include <QMoveEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>

#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <glm/glm.hpp>

#include <QOpenGLFunctions_3_3_Core> // for timer queries

#include "Atmosphere.h"
#include "DebugPainter.h"
#include "Framebuffer.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "TileManager.h"
#include "Window.h"
#include "helpers.h"
#include "nucleus/utils/bit_coding.h"
#include "TimerManager.h"

using gl_engine::Window;
using gl_engine::UniformBuffer;

Window::Window()
    : m_camera({ 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 }) // should point right at the stephansdom
{
    qDebug("Window::Window()");
    m_tile_manager = std::make_unique<TileManager>();
    QTimer::singleShot(1, [this]() { emit update_requested(); });
}

Window::~Window()
{
    qDebug("~Window::Window()");
}

void Window::initialise_gpu()
{
    QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
    logger->initialize();
    connect(logger, &QOpenGLDebugLogger::messageLogged, [](const auto& message) {
        qDebug() << message;
    });
    logger->disableMessages(QList<GLuint>({ 131185 }));
    logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    m_debug_painter = std::make_unique<DebugPainter>();
    m_shader_manager = std::make_unique<ShaderManager>();
    m_atmosphere = std::make_unique<Atmosphere>();

    m_tile_manager->init();
    m_tile_manager->initilise_attribute_locations(m_shader_manager->tile_shader());
    m_screen_quad_geometry = gl_engine::helpers::create_screen_quad_geometry();
    m_framebuffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Int24, std::vector({ Framebuffer::ColourFormat::RGBA8 }));
    m_depth_buffer = std::make_unique<Framebuffer>(Framebuffer::DepthFormat::Int24, std::vector({ Framebuffer::ColourFormat::RGBA8 }));

    m_shared_config_ubo = std::make_unique<gl_engine::UniformBuffer<gl_engine::uboSharedConfig>>(0, "shared_config");
    m_shared_config_ubo->init();
    m_shared_config_ubo->bind_to_shader(m_shader_manager->tile_shader());

    m_timer = std::make_unique<gl_engine::TimerManager>();
    m_timer->add_timer("frame", gl_engine::TimerTypes::CPU, "CPU");
    m_timer->add_timer("depth", gl_engine::TimerTypes::GPUAsync, "GPU");
    m_timer->add_timer("atmosphere", gl_engine::TimerTypes::GPUAsync, "GPU");
    m_timer->add_timer("tiles", gl_engine::TimerTypes::GPUAsync, "GPU", 120);
    m_timer->add_timer("compose", gl_engine::TimerTypes::GPUAsync, "GPU");
    m_timer->add_timer("tile_generator", gl_engine::TimerTypes::CPU, "CPU");

    emit gpu_ready_changed(true);
}

void Window::resize_framebuffer(int width, int height)
{
    if (width == 0 || height == 0)
        return;

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    m_framebuffer->resize({ width, height });
    m_atmosphere->resize({ width, height });
    m_depth_buffer->resize({ width / 4, height / 4 });

    f->glViewport(0, 0, width, height);
}

void Window::paint(QOpenGLFramebufferObject* framebuffer)
{
    m_timer->start_timer("frame");

    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

    f->glEnable(GL_CULL_FACE);
    f->glCullFace(GL_BACK);

    m_camera.set_viewport_size(m_framebuffer->size());

    // DEPTH BUFFER
    m_depth_buffer->bind();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);

    m_shader_manager->depth_program()->bind();
    m_timer->start_timer("depth");
    m_tile_manager->draw(m_shader_manager->depth_program(), m_camera, m_timer.get());
    m_timer->stop_timer("depth");
    m_depth_buffer->unbind();
    // END DEPTH BUFFER


    m_framebuffer->bind();
    f->glClearColor(1.0, 0.0, 0.5, 1);

    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_shader_manager->atmosphere_bg_program()->bind();
    m_timer->start_timer("atmosphere");
    m_atmosphere->draw(m_shader_manager->atmosphere_bg_program(),
                       m_camera,
                       m_shader_manager->screen_quad_program(),
                       m_framebuffer.get());
    m_timer->stop_timer("atmosphere");
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_shader_manager->tile_shader()->bind();
    m_timer->start_timer("tiles");
    m_tile_manager->draw(m_shader_manager->tile_shader(), m_camera, m_timer.get());
    m_timer->stop_timer("tiles");

    m_framebuffer->unbind();
    if (framebuffer)
        framebuffer->bind();

    m_shader_manager->screen_quad_program()->bind();
    m_framebuffer->bind_colour_texture(0);
    m_timer->start_timer("compose");
    m_screen_quad_geometry.draw();
    m_timer->stop_timer("compose");

    m_shader_manager->release();


    m_timer->stop_timer("frame");

    // Everything should be available by now, because we are synchronising with glFinish
    QList<gl_engine::qTimerReport> new_values = m_timer->fetch_results();
    if (new_values.size() > 0) {
        emit report_measurements(new_values);
    }
}

void Window::paintOverGL(QPainter* painter)
{
    const auto frame_duration_text = QString("draw indicator: ");

    const auto random_u32 = QRandomGenerator::global()->generate();

    painter->setFont(QFont("Helvetica", 12));
    painter->setPen(Qt::white);
    QRect text_bb = painter->boundingRect(10, 20, 1, 15, Qt::TextSingleLine, frame_duration_text);
    painter->drawText(10, 20, frame_duration_text);
    painter->drawText(10, 40, m_debug_scheduler_stats);
    painter->drawText(10, 60, m_debug_text);
    painter->setBrush(QBrush(QColor(random_u32)));
    painter->drawRect(int(text_bb.right()) + 5, 8, 12, 12);
}

void Window::shared_config_changed(gl_engine::uboSharedConfig ubo) {
    m_shared_config_ubo->data = ubo;
    //m_shared_config_ubo->data.setProperty("sun_light", QVector4D(2.0, 0.0, 0.0, newIntensity));
    //m_shared_config_ubo->data.m_sun_light[3] = newIntensity;
    m_shared_config_ubo->update_gpu_data();
    emit update_requested();
}

void Window::key_press(const QKeyCombination& e) {
    QKeyEvent ev = QKeyEvent(QEvent::Type::KeyPress, e.key(), e.keyboardModifiers());
    this->keyPressEvent(&ev);
}

void Window::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key::Key_F5) {
        m_shader_manager->reload_shaders();
        qDebug("all shaders reloaded");
        emit update_requested();
    }
    if (e->key() == Qt::Key::Key_F11
        || (e->key() == Qt::Key_P && e->modifiers() == Qt::ControlModifier)
        || (e->key() == Qt::Key_F5 && e->modifiers() == Qt::ControlModifier)) {
        e->ignore();
    }

    emit key_pressed(e->keyCombination());
}

void Window::keyReleaseEvent(QKeyEvent* e)
{
    emit key_released(e->keyCombination());
}

void Window::updateCameraEvent()
{
    emit update_camera_requested();
}

void Window::set_permissible_screen_space_error(float new_error)
{
    if (m_tile_manager)
        m_tile_manager->set_permissible_screen_space_error(new_error);
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

void Window::update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    assert(m_tile_manager);
    m_tile_manager->update_gpu_quads(new_quads, deleted_quads);
}

float Window::depth(const glm::dvec2& normalised_device_coordinates)
{
    const auto read_float = nucleus::utils::bit_coding::to_f16f16(m_depth_buffer->read_colour_attachment_pixel(0, normalised_device_coordinates))[0];
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
    m_debug_painter.reset();
    m_atmosphere.reset();
    m_shader_manager.reset();
    m_framebuffer.reset();
    m_depth_buffer.reset();
    m_screen_quad_geometry = {};
}

void Window::set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator)
{
    assert(m_tile_manager);
    m_tile_manager->set_aabb_decorator(new_aabb_decorator);
}

void Window::remove_tile(const tile::Id& id)
{
    assert(m_tile_manager);
    m_tile_manager->remove_tile(id);
}

nucleus::camera::AbstractDepthTester* Window::depth_tester()
{
    return this;
}
