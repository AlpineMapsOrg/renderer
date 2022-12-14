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
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>

#include "GLWindow.h"
#include "alpine_gl_renderer/GLTileManager.h"
#include "nucleus/TileLoadService.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/camera/CrapyInteraction.h"
#include "nucleus/camera/NearPlaneAdjuster.h"
#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/GpuCacheTileScheduler.h"
#include "nucleus/tile_scheduler/SimplisticTileScheduler.h"
#include "nucleus/tile_scheduler/utils.h"
#include "qnetworkreply.h"
#include "sherpa/TileHeights.h"

// This example demonstrates easy, cross-platform usage of OpenGL ES 3.0 functions via
// QOpenGLExtraFunctions in an application that works identically on desktop platforms
// with OpenGL 3.3 and mobile/embedded devices with OpenGL ES 3.0.

// The code is always the same, with the exception of two places: (1) the OpenGL context
// creation has to have a sufficiently high version number for the features that are in
// use, and (2) the shader code's version directive is different.

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setOption(QSurfaceFormat::DebugContext);

    bool running_in_browser = false;
    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
        running_in_browser = true;
    }

    QSurfaceFormat::setDefaultFormat(fmt);

    TileLoadService terrain_service("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
//    TileLoadService ortho_service("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
    TileLoadService ortho_service("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg", { "", "1", "2", "3", "4" });
    GpuCacheTileScheduler scheduler;
    scheduler.set_gpu_cache_size(1000);

    GLWindow glWindow;
    if (running_in_browser)
        glWindow.showFullScreen();
    else
        glWindow.showMaximized();

    QNetworkAccessManager m_network_manager;
    QNetworkReply* reply = m_network_manager.get(QNetworkRequest(QUrl("https://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb")));
//    QNetworkReply* reply = m_network_manager.get(QNetworkRequest(QUrl("https://alpinemaps.cg.tuwien.ac.at/threaded//height_data.atb")));
    QObject::connect(reply, &QNetworkReply::finished, &glWindow, [reply, &scheduler, &glWindow, &app]() {
        const auto url = reply->url();
        const auto error = reply->error();
        if (error == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            const auto decorator = tile_scheduler::AabbDecorator::make(TileHeights::deserialise(data));
            QTimer::singleShot(1, &scheduler, [&scheduler, decorator]() { scheduler.set_aabb_decorator(decorator); });

            assert(glWindow.gpuTileManager());
            glWindow.gpuTileManager()->set_aabb_decorator(decorator);
        } else {
            qDebug() << "Loading of " << url << " failed: " << error;
            QGuiApplication::exit(0);
            // do we need better error handling?
        }
        reply->deleteLater();
    });

    camera::Controller camera_controller { camera::stored_positions::westl_hochgrubach_spitze() };
//    camera::Controller camera_controller { camera::stored_positions::stephansdom() };
    camera_controller.set_interaction_style(std::make_unique<camera::CrapyInteraction>());

    camera::NearPlaneAdjuster near_plane_adjuster;

#ifdef ALP_ENABLE_THREADING
    QThread scheduler_thread;
//    terrain_service.moveToThread(&scheduler_thread);
//    ortho_service.moveToThread(&scheduler_thread);
    scheduler.moveToThread(&scheduler_thread);
    scheduler_thread.start();
#endif

    QObject::connect(&glWindow, &GLWindow::viewport_changed, &camera_controller, &camera::Controller::setViewport);
    QObject::connect(&glWindow, &GLWindow::mouse_moved, &camera_controller, &camera::Controller::mouse_move);
    QObject::connect(&glWindow, &GLWindow::mouse_pressed, &camera_controller, &camera::Controller::mouse_press);
    QObject::connect(&glWindow, &GLWindow::wheel_turned, &camera_controller, &camera::Controller::wheel_turn);
    QObject::connect(&glWindow, &GLWindow::key_pressed, &camera_controller, &camera::Controller::key_press);
    QObject::connect(&glWindow, &GLWindow::touch_made, &camera_controller, &camera::Controller::touch);
    QObject::connect(&glWindow, &GLWindow::key_pressed, &scheduler, &TileScheduler::key_press);

    QObject::connect(&camera_controller, &camera::Controller::definitionChanged, &scheduler, &TileScheduler::updateCamera);
    QObject::connect(&camera_controller, &camera::Controller::definitionChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::updateCamera);
    QObject::connect(&camera_controller, &camera::Controller::definitionChanged, &glWindow, &GLWindow::update_camera);

    QObject::connect(&scheduler, &TileScheduler::tileRequested, &terrain_service, &TileLoadService::load);
    QObject::connect(&scheduler, &TileScheduler::tileRequested, &ortho_service, &TileLoadService::load);
    QObject::connect(&scheduler, &TileScheduler::tileReady, &glWindow, [&glWindow](const std::shared_ptr<Tile>& tile) { glWindow.gpuTileManager()->addTile(tile); });
    QObject::connect(&scheduler, &TileScheduler::tileReady, &near_plane_adjuster, &camera::NearPlaneAdjuster::addTile);
    QObject::connect(&scheduler, &TileScheduler::tileReady, &glWindow, qOverload<>(&GLWindow::update));
    QObject::connect(&scheduler, &TileScheduler::tileExpired, &glWindow, [&glWindow](const auto& tile) { glWindow.gpuTileManager()->removeTile(tile); });
    QObject::connect(&scheduler, &TileScheduler::tileExpired, &near_plane_adjuster, &camera::NearPlaneAdjuster::removeTile);
    QObject::connect(&scheduler, &TileScheduler::debug_scheduler_stats_updated, &glWindow, &GLWindow::update_debug_scheduler_stats);

    QObject::connect(&ortho_service, &TileLoadService::loadReady, &scheduler, &TileScheduler::receiveOrthoTile);
    QObject::connect(&ortho_service, &TileLoadService::tileUnavailable, &scheduler, &TileScheduler::notifyAboutUnavailableOrthoTile);
    QObject::connect(&terrain_service, &TileLoadService::loadReady, &scheduler, &TileScheduler::receiveHeightTile);
    QObject::connect(&terrain_service, &TileLoadService::tileUnavailable, &scheduler, &TileScheduler::notifyAboutUnavailableHeightTile);

    QObject::connect(&near_plane_adjuster, &camera::NearPlaneAdjuster::nearPlaneChanged, &camera_controller, &camera::Controller::setNearPlane);

    // in web assembly, the gl window is resized before it is connected. need to set viewport manually.
    // native, however, glWindow has a zero size at this point.
    if (glWindow.width() > 0 && glWindow.height() > 0)
        camera_controller.setViewport({ glWindow.width(), glWindow.height() });
    camera_controller.update();

    return QGuiApplication::exec();
}
