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
#include "nucleus/Controller.h"
#include "nucleus/camera/Controller.h"


// This example demonstrates easy, cross-platform usage of OpenGL ES 3.0 functions via
// QOpenGLExtraFunctions in an application that works identically on desktop platforms
// with OpenGL 3.3 and mobile/embedded devices with OpenGL ES 3.0.

// The code is always the same, with the exception of two places: (1) the OpenGL context
// creation has to have a sufficiently high version number for the features that are in
// use, and (2) the shader code's version directive is different.

int main(int argc, char* argv[])
{
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

    Window glWindow;
    nucleus::Controller controller(glWindow.render_window());

    QObject::connect(&glWindow, &Window::mouse_moved, controller.camera_controller(), &nucleus::camera::Controller::mouse_move);
    QObject::connect(&glWindow, &Window::mouse_pressed, controller.camera_controller(), &nucleus::camera::Controller::mouse_press);
    QObject::connect(&glWindow, &Window::wheel_turned, controller.camera_controller(), &nucleus::camera::Controller::wheel_turn);
    QObject::connect(&glWindow, &Window::touch_made, controller.camera_controller(), &nucleus::camera::Controller::touch);
    QObject::connect(&glWindow, &Window::resized, controller.camera_controller(), [&controller](glm::uvec2 new_size) {
        controller.camera_controller()->set_viewport(new_size);
    });

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
    glWindow.showMaximized();
#else
    glWindow.show();
#endif

    // in web assembly, the gl window is resized before it is connected. need to set viewport manually.
    // native, however, glWindow has a zero size at this point.
    if (glWindow.width() > 0 && glWindow.height() > 0)
        controller.camera_controller()->set_viewport({ glWindow.width(), glWindow.height() });

    return QGuiApplication::exec();
}
