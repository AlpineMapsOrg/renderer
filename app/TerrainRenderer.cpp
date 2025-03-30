/*****************************************************************************
 * AlpineMaps.org
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

#include "RenderingContext.h"
#include "TerrainRendererItem.h"
#include <gl_engine/Context.h>
#include <gl_engine/Window.h>
#include <nucleus/EngineContext.h>
#include <nucleus/camera/Controller.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/map_label/Filter.h>
#include <nucleus/map_label/Scheduler.h>
#include <nucleus/picker/PickerManager.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/TextureScheduler.h>
#include <nucleus/utils/thread.h>

TerrainRenderer::TerrainRenderer()
{
    using nucleus::map_label::Filter;
    using nucleus::picker::PickerManager;
    using Scheduler = nucleus::tile::Scheduler;
    using CameraController = nucleus::camera::Controller;

    auto* ctx = RenderingContext::instance();
    ctx->initialise();
    m_glWindow = std::make_unique<gl_engine::Window>(ctx->engine_context());

    auto* gl_window_ptr = m_glWindow.get();

    m_camera_controller = std::make_unique<CameraController>(nucleus::camera::PositionStorage::instance()->get("grossglockner"), m_glWindow.get(), ctx->data_querier().get());

    // clang-format off
    // NOTICE ME!!!! READ THIS, IF YOU HAVE TROUBLES WITH SIGNALS NOT REACHING THE QML RENDERING THREAD!!!!111elevenone
    // In Qt/QML the rendering thread goes to sleep (at least until Qt 6.5, See RenderThreadNotifier).
    // At the time of writing, an additional connection from tile_ready and tile_expired to the notifier is made.
    // this only works if ALP_ENABLE_THREADING is on, i.e., the tile scheduler is on an extra thread. -> potential issue on webassembly
    connect(m_camera_controller.get(), &CameraController::definition_changed, ctx->geometry_scheduler(),   &Scheduler::update_camera);
    connect(m_camera_controller.get(), &CameraController::definition_changed, ctx->map_label_scheduler(),  &Scheduler::update_camera);
    connect(m_camera_controller.get(), &CameraController::definition_changed, ctx->ortho_scheduler(),      &Scheduler::update_camera);
    connect(m_camera_controller.get(), &CameraController::definition_changed, m_glWindow.get(),            &gl_engine::Window::update_camera);

    connect(ctx->geometry_scheduler(), &nucleus::tile::GeometryScheduler::gpu_tiles_updated, gl_window_ptr, &gl_engine::Window::update_requested);
    connect(ctx->ortho_scheduler(),    &nucleus::tile::TextureScheduler::gpu_tiles_updated,  gl_window_ptr, &gl_engine::Window::update_requested);
    connect(ctx->label_filter().get(), &Filter::filter_finished,                             gl_window_ptr, &gl_engine::Window::update_requested);

    connect(ctx->picker_manager().get(),   &PickerManager::pick_requested,     gl_window_ptr,                  &gl_engine::Window::pick_value);
    connect(gl_window_ptr,                 &gl_engine::Window::value_picked,   ctx->picker_manager().get(),    &PickerManager::eval_pick);
    // clang-format on

    m_glWindow->initialise_gpu();
    // ctx->scheduler()->set_enabled(true); // after tile manager moves to ctx.
}

TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::synchronize(QQuickFramebufferObject *item)
{
    // warning:
    // you can only safely copy objects between main and render thread.
    // the tile scheduler is in an extra thread, there will be races if you write to it.
    m_window = item->window();
    TerrainRendererItem* i = static_cast<TerrainRendererItem*>(item);
    m_camera_controller->set_pixel_error_threshold(1.0 / i->settings()->render_quality());
    m_camera_controller->set_viewport({ i->width(), i->height() });
    m_camera_controller->set_field_of_view(i->field_of_view());

    auto cameraFrontAxis = m_camera_controller->definition().z_axis();
    auto degFromNorth = glm::degrees(glm::acos(glm::dot(glm::normalize(glm::dvec3(cameraFrontAxis.x, cameraFrontAxis.y, 0)), glm::dvec3(0, -1, 0))));
    if (cameraFrontAxis.x > 0) {
        i->set_camera_rotation_from_north(degFromNorth);
    } else {
        i->set_camera_rotation_from_north(-degFromNorth);
    }

    const auto oc = m_camera_controller->operation_centre();
    const auto oc_distance = m_camera_controller->operation_centre_distance();
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

    if (!(i->camera() == m_camera_controller->definition())) {
        const auto tmp_camera = m_camera_controller->definition();
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

nucleus::camera::Controller* TerrainRenderer::controller() const { return m_camera_controller.get(); }
