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
#include <QTranslator>

#include "CameraTransformationProxyModel.h"
#include "GnssInformation.h"
#include "MapLabelModel.h"
#include "RenderThreadNotifier.h"
#include "myframebufferobject.h"

int main(int argc, char **argv)
{
    //    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGLRhi);
    QGuiApplication app(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = QLocale::languageToCode(QLocale(locale).language());
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

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

    qmlRegisterType<MyFrameBufferObject>("Alpine", 42, 0, "MeshRenderer");
    qmlRegisterType<GnssInformation>("Alpine", 42, 0, "GnssInformation");
    qmlRegisterType<MapLabelModel>("Alpine", 42, 0, "LabelModel");
    qmlRegisterType<CameraTransformationProxyModel>("Alpine", 42, 0, "CameraTransformationProxyModel");

    QQmlApplicationEngine engine;

    RenderThreadNotifier::instance();
    const QUrl url(u"qrc:/alpinemaps/app/main.qml"_qs);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [](QObject* obj, const QUrl& objUrl) {
            if (!obj) {
                qDebug() << "Creating QML object from " << objUrl << " failed!";
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);
    engine.load(url);
    QQuickWindow* root_window = dynamic_cast<QQuickWindow*>(engine.rootObjects().first());
    if (root_window == nullptr) {
        qDebug() << "root window not created!";
        return 1;
    }
    RenderThreadNotifier::instance()->set_root_window(root_window);

    return app.exec();
}

