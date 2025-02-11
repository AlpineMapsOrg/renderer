/*****************************************************************************
 * AlpineMaps.org
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
#if defined(ALP_ENABLE_DEV_TOOLS) || defined(__ANDROID__)
#include <QApplication>
#else
#include <QGuiApplication>
#endif
#ifdef ALP_ENABLE_DEV_TOOLS
#include "HotReloader.h"
#include "TimerFrontendManager.h"
#endif
#include "RenderThreadNotifier.h"
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

// QtMessageHandler originalHandler = nullptr;
// void filter_log(QtMsgType type, const QMessageLogContext& context, const QString& msg)
// {
//     if (msg.contains("QObject::killTimer: Timers cannot be stopped from another thread")) {
//         qDebug() << "dudu";
//     }
//     if (msg.contains("Destroyed while thread is still running")) {
//         qDebug() << "aha!, current thread: " << QThread::currentThread()->objectName() << ": " << QThread::currentThread();
//     }
//     if (originalHandler) {
//         originalHandler(type, context, msg);
//     }
// }

int main(int argc, char **argv)
{
    // originalHandler = qInstallMessageHandler(filter_log);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGLRhi);
#if defined(ALP_ENABLE_DEV_TOOLS) || defined(__ANDROID__)
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

    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        qDebug("Requesting 3.3 core context");
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    } else {
        qDebug("Requesting 3.0 context");
        fmt.setVersion(3, 0);
    }

    QSurfaceFormat::setDefaultFormat(fmt);

    // create in main thread
#ifdef ALP_ENABLE_DEV_TOOLS
    TimerFrontendManager::instance();
#endif
    RenderThreadNotifier::instance();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("_r", ALP_QML_SOURCE_DIR);
    engine.rootContext()->setContextProperty("_positionList", QVariant::fromValue(nucleus::camera::PositionStorage::instance()->getPositionList()));
    engine.rootContext()->setContextProperty("_alpine_renderer_version", QString::fromStdString(nucleus::version()));

    auto track_model = TrackModel();
    engine.rootContext()->setContextProperty("_track_model", &track_model);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [](QObject* obj, const QUrl& objUrl) {
            if (!obj) {
                qDebug() << "Creating QML object from " << objUrl << " failed!";
                QCoreApplication::exit(-1);
            }
            // qDebug() << "QQmlApplicationEngine::objectCreated: " << obj;
        },
        Qt::QueuedConnection);

#ifdef ALP_ENABLE_DEV_TOOLS
    HotReloader hotreloader(&engine, ALP_QML_SOURCE_DIR);
    engine.rootContext()->setContextProperty("_hotreloader", &hotreloader);
    engine.rootContext()->setContextProperty("_debug_gui", true);
    engine.load(QUrl(ALP_QML_SOURCE_DIR "loader_dev.qml"));
#else
    engine.rootContext()->setContextProperty("_debug_gui", false);
    engine.load(QUrl(ALP_QML_SOURCE_DIR "loader.qml"));
#endif
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

    RenderThreadNotifier::instance()->set_root_window(root_window);

    return app.exec();
}

