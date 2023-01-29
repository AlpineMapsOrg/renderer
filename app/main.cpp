/****************************************************************************
**
** Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company.
** Author: Giuseppe D'Angelo
** Contact: info@kdab.com
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickView>
#include <QRunnable>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>

#include "RenderThreadNotifier.h"
#include "myframebufferobject.h"

int main(int argc, char **argv)
{
    //    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGLRhi);
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

    qmlRegisterType<MyFrameBufferObject>("MyRenderLibrary", 42, 0, "MeshRenderer");

    QQmlApplicationEngine engine;
    //    QTimer timer;
    //    timer.setInterval(5);
    //    timer.setSingleShot(false);
    //    timer.start();

    RenderThreadNotifier::instance();
    const QUrl url(u"qrc:/alpinemaps/app/main.qml"_qs);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url, &engine /*, &timer*/](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
            //            if (url != objUrl)
            //                return;
            //            QQuickWindow* root_window = dynamic_cast<QQuickWindow*>(engine.rootObjects().first());
            //                        if (root_window == nullptr)
            //                return;
            //            qDebug() << "QQmlApplicationEngine::objectCreated current thread: " << QThread::currentThread();
            //            RenderThreadNotifier::instance()->set_root_window(root_window);

            //            QObject::connect(&timer, &QTimer::timeout, root_window, [root_window]() {
            //                auto* runnable = QRunnable::create([]() {
            //                    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            //                });
            //                root_window->scheduleRenderJob(runnable, QQuickWindow::RenderStage::NoStage);
            //            });
        },
        Qt::QueuedConnection);
    engine.load(url);
    QQuickWindow* root_window = dynamic_cast<QQuickWindow*>(engine.rootObjects().first());
    if (root_window == nullptr) {
        qDebug() << "root window not created!";
        return 1;
    }
    RenderThreadNotifier::instance()->set_root_window(root_window);

    QTimer::singleShot(10, []() { RenderThreadNotifier::instance()->notify(); });
    QTimer::singleShot(100, []() { RenderThreadNotifier::instance()->notify(); }); // dirty code to notify the render thread of the height data being loaded.
    QTimer::singleShot(500, []() { RenderThreadNotifier::instance()->notify(); });
    QTimer::singleShot(1000, []() { RenderThreadNotifier::instance()->notify(); });

    //    QQuickView view;
    //    view.setResizeMode(QQuickView::SizeRootObjectToView);
    //    view.setSource(QUrl("qrc:///qml/main.qml"));
    //    view.resize(600, 600);
    //    view.show();

    return app.exec();
}

