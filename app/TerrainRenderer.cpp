/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "TerrainRenderer.h"

#include <QDateTime>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QThread>

#include <gl_engine/Context.h>
#include <gl_engine/Window.h>
#include <nucleus/Controller.h>
#include <nucleus/EngineContext.h>
#include <nucleus/camera/Controller.h>
#include <nucleus/tile_scheduler/Scheduler.h>
#include <nucleus/utils/thread.h>

#include "TerrainRendererItem.h"

TerrainRenderer::TerrainRenderer()
{
    auto& context = gl_engine::Context::instance();
    auto* render_thread = QThread::currentThread();
    connect(render_thread, &QThread::finished, &context, &nucleus::EngineContext::destroy);

    // not ideal:
    // the engine context needs to live on the render thread.
    // however, (currently) we need it before the render thread is created for signal slot connections.
    // so we need to move it afterwards. and it can be only moved from its own thread
    nucleus::utils::thread::async_call(&context, [render_thread]() { gl_engine::Context::instance().moveToThread(render_thread); });
    if (!context.is_alive())
        context.initialise();

    m_glWindow = std::make_unique<gl_engine::Window>();
    m_controller = std::make_unique<nucleus::Controller>(m_glWindow.get());
    m_controller->tile_scheduler()->set_ortho_tile_compression_algorithm(m_glWindow->ortho_tile_compression_algorithm());
    m_glWindow->initialise_gpu();
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("TerrainRendererItemRenderer()");
#endif
}

TerrainRenderer::~TerrainRenderer()
{
#ifdef ALP_ENABLE_TRACK_OBJECT_LIFECYCLE
    qDebug("~TerrainRenderer()");
#endif
}

void TerrainRenderer::synchronize(QQuickFramebufferObject *item)
{
    // warning:
    // you can only safely copy objects between main and render thread.
    // the tile scheduler is in an extra thread, there will be races if you write to it.
    m_window = item->window();
    TerrainRendererItem* i = static_cast<TerrainRendererItem*>(item);
    //        m_controller->camera_controller()->set_virtual_resolution_factor(i->render_quality());
    m_glWindow->set_permissible_screen_space_error(1.0 / i->settings()->render_quality());
    m_controller->camera_controller()->set_viewport({ i->width(), i->height() });
    m_controller->camera_controller()->set_field_of_view(i->field_of_view());

    auto cameraFrontAxis = m_controller->camera_controller()->definition().z_axis();
    auto degFromNorth = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::dvec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::dvec3(0, -1, 0))));
    if (cameraFrontAxis.x > 0) {
        i->set_camera_rotation_from_north(degFromNorth);
    } else {
        i->set_camera_rotation_from_north(-degFromNorth);
    }

    const auto oc = m_controller->camera_controller()->operation_centre();
    const auto oc_distance = m_controller->camera_controller()->operation_centre_distance();
    if (oc.has_value()) {
        i->set_camera_operation_centre_visibility(true);
        i->set_camera_operation_centre(QPointF(oc.value().x, oc.value().y));
        if (oc_distance.has_value()) {
            i->set_camera_operation_centre_distance(oc_distance.value());
        } else {
            i->set_camera_operation_centre_distance(-1);
        }
    } else {
        i->set_camera_operation_centre_visibility(false);
    }

    if (!(i->camera() == m_controller->camera_controller()->definition())) {
        const auto tmp_camera = m_controller->camera_controller()->definition();
        nucleus::utils::thread::async_call(i, [i, tmp_camera]() {
            i->set_read_only_camera(tmp_camera);
            i->set_read_only_camera_width(tmp_camera.viewport_size().x);
            i->set_read_only_camera_height(tmp_camera.viewport_size().y);
        });
    }
}

void TerrainRenderer::render()
{
    m_window->beginExternalCommands();
    m_glWindow->paint(this->framebufferObject());
    m_window->endExternalCommands();
    //    qDebug() << "TerrainRenderer::render: " << QDateTime::currentDateTime().time().toString("ss.zzz");
}

QOpenGLFramebufferObject *TerrainRenderer::createFramebufferObject(const QSize &size)
{
    qDebug() << "QOpenGLFramebufferObject *createFramebufferObject(const QSize& " << size << ")";
    m_window->beginExternalCommands();
    m_glWindow->resize_framebuffer(size.width(), size.height());
    m_window->endExternalCommands();
    QOpenGLFramebufferObjectFormat format;
    format.setSamples(1);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    return new QOpenGLFramebufferObject(size, format);
}

gl_engine::Window *TerrainRenderer::glWindow() const
{
    return m_glWindow.get();
}

nucleus::Controller *TerrainRenderer::controller() const
{
    return m_controller.get();
}
