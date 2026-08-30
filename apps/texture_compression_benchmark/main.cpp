/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGLRhi);

    QSurfaceFormat format;
#if QT_CONFIG(opengles2)
    format.setVersion(3, 0);
#else
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
#endif
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AlpineMaps.org"));
    QCoreApplication::setApplicationName(QStringLiteral("TextureCompressionPreview"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Texture Compression Preview"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Roboto/Roboto-Regular.ttf"));
    application.setFont(QFont(QStringLiteral("Roboto"), 12, QFont::Normal));

    QQmlApplicationEngine engine;
    engine.loadFromModule("TextureCompressionPreview", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    return application.exec();
}
