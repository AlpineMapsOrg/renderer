/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2023 madam
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

#include "Window.h"

Window::Window()
{
    m_gl_window = new gl_engine::Window();
    connect(m_gl_window, &gl_engine::Window::update_requested, this, qOverload<>(&QOpenGLWindow::update));
    connect(m_timer, &QTimer::timeout, this, &Window::key_timer);
}

void Window::initializeGL()
{
    m_gl_window->initialise_gpu();
}

void Window::resizeGL(int w, int h)
{
    if (m_gl_window) {
        m_gl_window->resize_framebuffer(w * devicePixelRatio(), h * devicePixelRatio());
        emit resized({ w, h });
    }
}

void Window::paintGL()
{
    m_gl_window->paint();
}

void Window::paintOverGL()
{
    QPainter p(this);
    m_gl_window->paintOverGL(&p);
}

gl_engine::Window* Window::render_window()
{
    return m_gl_window;
}

void Window::closeEvent(QCloseEvent* e) {
    // NOTE: The following fixes the bug where the plain_renderer crashes if m_gl_window was set as a direct member variable
    // For some reason in this case the QOpenGLContext was not available anymore before deleting the m_gl_window. Thats why it crashed
    // when exiting. Deleting m_gl_window manually inside the closeEvent fixes this bug:
    delete m_gl_window;
    m_gl_window = nullptr;
}

void Window::mousePressEvent(QMouseEvent* e)
{
    emit mouse_pressed(nucleus::event_parameter::make(e));
}

void Window::mouseMoveEvent(QMouseEvent* e)
{
    emit mouse_moved(nucleus::event_parameter::make(e));
}

void Window::wheelEvent(QWheelEvent* e)
{
    emit wheel_turned(nucleus::event_parameter::make(e));
}

void Window::keyPressEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    m_keys_pressed++;
    if (!m_timer->isActive()) {
        m_timer->start(1000.0f / 30.0f);
    }
    if (m_gl_window)
        m_gl_window->keyPressEvent(e);
}

void Window::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    m_keys_pressed--;
    if (m_keys_pressed <= 0) {
        m_timer->stop();
    }
    if (m_gl_window) m_gl_window->keyReleaseEvent(e);
}

void Window::touchEvent(QTouchEvent* e)
{
    emit touch_made(nucleus::event_parameter::make(e));
}

void Window::key_timer()
{
    if (m_gl_window) m_gl_window->updateCameraEvent();
}
