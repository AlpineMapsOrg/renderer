/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company (Giuseppe D'Angelo)
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

#include <QDirIterator>
#include <QFontDatabase>
#if defined(ALP_ENABLE_DEBUG_GUI) || defined(__ANDROID__)
#include <QApplication>
#else
#include <QGuiApplication>
#endif
#include "GnssInformation.h"
#include "HotReloader.h"
#include "ModelBinding.h"
#include "RenderThreadNotifier.h"
#include "TerrainRendererItem.h"
#include "TrackModel.h"
#include <QLoggingCategory>
#include <QNetworkInformation>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QRunnable>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <gl_engine/Context.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/version.h>

int main(int argc, char **argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGLRhi);
#if defined(ALP_ENABLE_DEBUG_GUI) || defined(__ANDROID__)
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    app.setWindowIcon(QIcon(":/icons/favicon.ico"));
    QCoreApplication::setOrganizationName("AlpineMaps.org");
    QCoreApplication::setApplicationName("AlpineApp");
    QGuiApplication::setApplicationDisplayName("Alpine Maps");
    QNetworkInformation::loadDefaultBackend(); // load here, so it sits on the correct thread.

    QFontDatabase::addApplicationFont(":/fonts/Roboto/Roboto-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Roboto/Roboto-Bold.ttf");
    app.setFont(QFont("Roboto", 12, 400));

#ifndef NDEBUG
    //    QLoggingCategory::setFilterRules("*.debug=true\n"
    //                                     "qt.qpa.fonts=true");
    // output qrc files:
    {
        qDebug() << "qrc files:";
        QDirIterator it(":", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto path = it.next();
            const auto file = QFile(path);
            qDebug() << path << " size: " << file.size() / 1024 << "kb";
        }
    }
#ifdef __EMSCRIPTEN__
    {
        qDebug() << "packaged files:";
        QDirIterator it("/", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto path = it.next();
            const auto file = QFile(path);
            qDebug() << path << " size: " << file.size() / 1024 << "kb";
        }
    }
#endif
    qDebug() << "Available fonts:";
    for (const auto& family : QFontDatabase::families()) {
        for (const auto& style : QFontDatabase::styles(family))
            qDebug() << family << "|" << style;
    }
#endif

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

    // Request OpenGL 3.3 core or OpenGL ES 3.0.
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
    }
    gl_engine::Context::instance(); // initialise, so it's ready when we create dependent objects. // still needs to be moved to the render thread.

    QSurfaceFormat::setDefaultFormat(fmt);

    QQmlApplicationEngine engine;

    HotReloader hotreloader(&engine, ALP_QML_SOURCE_DIR);
    engine.rootContext()->setContextProperty("_hotreloader", &hotreloader);
    engine.rootContext()->setContextProperty("_r", ALP_QML_SOURCE_DIR);
    engine.rootContext()->setContextProperty("_positionList", QVariant::fromValue(nucleus::camera::PositionStorage::instance()->getPositionList()));
    engine.rootContext()->setContextProperty("_alpine_renderer_version", QString::fromStdString(nucleus::version()));

#ifdef ALP_ENABLE_DEBUG_GUI
    engine.rootContext()->setContextProperty("_debug_gui", true);
#else
    engine.rootContext()->setContextProperty("_debug_gui", false);
#endif
    auto track_model = TrackModel();
    engine.rootContext()->setContextProperty("_track_model", &track_model);

    RenderThreadNotifier::instance();
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [](QObject* obj, const QUrl& objUrl) {
            if (!obj) {
                qDebug() << "Creating QML object from " << objUrl << " failed!";
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);
    engine.load(QUrl(ALP_QML_SOURCE_DIR "main_loader.qml"));
    QQuickWindow* root_window = dynamic_cast<QQuickWindow*>(engine.rootObjects().first());
    if (root_window == nullptr) {
        qDebug() << "root window not created!";
        return 1;
    }

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
    root_window->showMaximized();
#endif
#ifdef ALP_APP_SHUTDOWN_AFTER_60S
    QTimer::singleShot(60000, &app, []() {
        qDebug() << "AlpineApp shuts down after 60s.";
        QGuiApplication::quit();
    });
#endif

    RenderThreadNotifier::instance()
        ->set_root_window(root_window);

    return app.exec();
}

