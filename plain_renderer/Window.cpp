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

#include <QRandomGenerator>

Window::Window(std::shared_ptr<gl_engine::Context> context)
{
    m_gl_window = new gl_engine::Window(context);
    connect(m_gl_window, &gl_engine::Window::update_requested, this, qOverload<>(&QOpenGLWindow::update));
}

void Window::initializeGL()
{
    emit initialisation_started();
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
    QPainter p(this);
    const auto random_u32 = QRandomGenerator::global()->generate();
    p.setBrush(QBrush(QColor(random_u32)));
    p.setPen(Qt::white);
    p.drawRect(8, 8, 16, 16);
}

gl_engine::Window* Window::render_window()
{
    return m_gl_window;
}

void Window::closeEvent(QCloseEvent*)
{
    // NOTE: The following fixes the bug where the plain_renderer crashes if m_gl_window was set as a direct member variable
    // For some reason in this case the QOpenGLContext was not available anymore before deleting the m_gl_window. Thats why it crashed
    // when exiting. Deleting m_gl_window manually inside the closeEvent fixes this bug:
    emit about_to_be_destoryed();
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
    emit key_pressed(e->keyCombination());
}

void Window::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) {
        return;
    }
    emit key_released(e->keyCombination());
}

void Window::touchEvent(QTouchEvent* e)
{
    emit touch_made(nucleus::event_parameter::make(e));
}

void Window::key_timer()
{
    if (m_gl_window)
        m_gl_window->update_requested();
}
