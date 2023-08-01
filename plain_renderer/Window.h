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

#pragma once

#include <QOpenGLWindow>
#include <QTimer>
#include <glm/glm.hpp>

#include "gl_engine/Window.h"
#include "nucleus/event_parameter.h"

class Window : public QOpenGLWindow
{
    Q_OBJECT
public:
    Window();

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintOverGL() override;

    [[nodiscard]] gl_engine::Window* render_window();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void touchEvent(QTouchEvent*) override;
    void closeEvent(QCloseEvent*) override;

signals:
    void mouse_pressed(const nucleus::event_parameter::Mouse&) const;
    void mouse_moved(const nucleus::event_parameter::Mouse&) const;
    void wheel_turned(const nucleus::event_parameter::Wheel&) const;
    void touch_made(const nucleus::event_parameter::Touch&) const;
    void resized(const glm::uvec2&) const;

private slots:
    void key_timer();

private:
    gl_engine::Window* m_gl_window = nullptr;
    QTimer *m_timer = new QTimer(this);
    int m_keys_pressed = 0;
    bool m_closing = false;
};

