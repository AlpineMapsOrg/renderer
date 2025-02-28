/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <iostream>

#include <QGuiApplication>
#include <QObject>
#include <QOpenGLContext>
#include <QScreen>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>

#include "Window.h"
#include <chrono>
#include <gl_engine/Context.h>
#include <gl_engine/Texture.h>
#include <gl_engine/TextureLayer.h>
#include <gl_engine/TileGeometry.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/camera/Controller.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/TileLoadService.h>
#include <nucleus/tile/setup.h>

#include <nucleus/tile/SchedulerDirector.h>

using namespace std::chrono_literals;
using nucleus::AbstractRenderWindow;
using nucleus::DataQuerier;
using Scheduler = nucleus::tile::Scheduler;
using GeometryScheduler = nucleus::tile::GeometryScheduler;
using TextureScheduler = nucleus::tile::TextureScheduler;
using nucleus::tile::TileLoadService;
using CameraController = nucleus::camera::Controller;

// QtMessageHandler originalHandler = nullptr;
// void filter_log(QtMsgType type, const QMessageLogContext& context, const QString& msg)
// {
//     if (msg.contains("QObject::killTimer: Timers cannot be stopped from another thread")) {
//         qDebug() << "dudu";
//     }
//     if (originalHandler) {
//         originalHandler(type, context, msg);
//     }
// }

int main(int argc, char** argv)
{
    // originalHandler = qInstallMessageHandler(filter_log);
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AlpineMaps.org");
    QCoreApplication::setApplicationName("PlainRenderer");

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setOption(QSurfaceFormat::DebugContext);

    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
    }

    QSurfaceFormat::setDefaultFormat(fmt);

    auto terrain_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    //    m_ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth,
    //    ".jpeg")); m_ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/",
    //    TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    // m_ortho_service.reset(new TileLoadService("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
    //                                           TileLoadService::UrlPattern::ZYX_yPointingSouth,
    //                                           ".jpeg",
    //                                           {"", "1", "2", "3", "4"}));
    auto ortho_service = std::make_unique<TileLoadService>("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");

    auto decorator = nucleus::tile::setup::aabb_decorator();
    QThread scheduler_thread;
    nucleus::tile::SchedulerDirector director;

    auto geometry_scheduler = nucleus::tile::setup::geometry_scheduler(std::move(terrain_service), decorator, &scheduler_thread);
    director.check_in("geometry", geometry_scheduler.scheduler);
    auto data_querier = std::make_shared<DataQuerier>(&geometry_scheduler.scheduler->ram_cache());

    auto ortho_scheduler = nucleus::tile::setup::texture_scheduler(std::move(ortho_service), decorator, &scheduler_thread);
    director.check_in("ortho", ortho_scheduler.scheduler);

    auto context = std::make_shared<gl_engine::Context>();
    context->set_tile_geometry(std::make_shared<gl_engine::TileGeometry>());
    context->set_ortho_layer(std::make_shared<gl_engine::TextureLayer>());
    context->tile_geometry()->set_quad_limit(512);
    context->tile_geometry()->set_aabb_decorator(decorator);
    context->ortho_layer()->set_quad_limit(512);

    Window glWindow(context);

    nucleus::camera::Controller camera_controller(nucleus::camera::PositionStorage::instance()->get("grossglockner"), glWindow.render_window(), data_querier.get());

    QObject::connect(&camera_controller, &CameraController::definition_changed, &glWindow, [&](const nucleus::camera::Definition&) {
        QTimer::singleShot(5ms, &camera_controller, &CameraController::advance_camera);
    });

    QObject::connect(&glWindow, &Window::initialisation_started, context.get(), [&]() { context->initialise(); });
    QObject::connect(&glWindow, &Window::initialisation_started, geometry_scheduler.scheduler.get(), [&]() { geometry_scheduler.scheduler->set_enabled(true); });
    QObject::connect(&glWindow, &Window::initialisation_started, &glWindow, [&ortho_scheduler]() {
        const auto compression_algorithm = gl_engine::Texture::compression_algorithm();
        nucleus::utils::thread::async_call(ortho_scheduler.scheduler.get(), [&ortho_scheduler, compression_algorithm]() {
            ortho_scheduler.scheduler->set_texture_compression_algorithm(compression_algorithm);
            ortho_scheduler.scheduler->set_enabled(true);
        });
    });
    QObject::connect(&camera_controller, &nucleus::camera::Controller::definition_changed, geometry_scheduler.scheduler.get(), &Scheduler::update_camera);
    QObject::connect(&camera_controller, &nucleus::camera::Controller::definition_changed, ortho_scheduler.scheduler.get(), &Scheduler::update_camera);
    QObject::connect(&camera_controller, &nucleus::camera::Controller::definition_changed, glWindow.render_window(), &AbstractRenderWindow::update_camera);
    QObject::connect(geometry_scheduler.scheduler.get(), &GeometryScheduler::gpu_quads_updated, context->tile_geometry(), &gl_engine::TileGeometry::update_gpu_quads);
    QObject::connect(geometry_scheduler.scheduler.get(), &GeometryScheduler::gpu_quads_updated, glWindow.render_window(), &AbstractRenderWindow::update_requested);
    QObject::connect(ortho_scheduler.scheduler.get(), &TextureScheduler::gpu_quads_updated, context->ortho_layer(), &gl_engine::TextureLayer::update_gpu_quads);
    QObject::connect(ortho_scheduler.scheduler.get(), &TextureScheduler::gpu_quads_updated, glWindow.render_window(), &AbstractRenderWindow::update_requested);

    QObject::connect(&glWindow, &Window::mouse_moved, &camera_controller, &nucleus::camera::Controller::mouse_move);
    QObject::connect(&glWindow, &Window::mouse_pressed, &camera_controller, &nucleus::camera::Controller::mouse_press);
    QObject::connect(&glWindow, &Window::wheel_turned, &camera_controller, &nucleus::camera::Controller::wheel_turn);
    QObject::connect(&glWindow, &Window::touch_made, &camera_controller, &nucleus::camera::Controller::touch);
    QObject::connect(&glWindow, &Window::key_pressed, &camera_controller, &nucleus::camera::Controller::key_press);
    QObject::connect(&glWindow, &Window::key_released, &camera_controller, &nucleus::camera::Controller::key_release);
    QObject::connect(&glWindow, &Window::resized, &camera_controller, [&camera_controller](glm::uvec2 new_size) { camera_controller.set_viewport(new_size); });
    QObject::connect(&glWindow, &Window::about_to_be_destoryed, context.get(), &gl_engine::Context::destroy);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, geometry_scheduler.scheduler.get(), [&]() {
        geometry_scheduler.scheduler.reset();
        ortho_scheduler.scheduler.reset();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, geometry_scheduler.tile_service.get(), [&]() {
        geometry_scheduler.tile_service.reset();
        ortho_scheduler.tile_service.reset();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&]() {
        scheduler_thread.wait(100); // msec
        scheduler_thread.quit();
        scheduler_thread.wait(500); // msec
    });

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
    glWindow.showMaximized();
#else
    glWindow.show();
#endif

    // in web assembly, the gl window is resized before it is connected. need to set viewport manually.
    // native, however, glWindow has a zero size at this point.
    if (glWindow.width() > 0 && glWindow.height() > 0)
        camera_controller.set_viewport({ glWindow.width(), glWindow.height() });

    scheduler_thread.start();

    return QGuiApplication::exec();
}
