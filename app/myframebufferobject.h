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

#ifndef MYFRAMEBUFFEROBJECT_H
#define MYFRAMEBUFFEROBJECT_H

#include "nucleus/event_parameter.h"
#include <QQuickFramebufferObject>

class MyFrameBufferObject : public QQuickFramebufferObject
{
    Q_OBJECT

    using EventParameters = std::variant<nucleus::event_parameter::Touch, nucleus::event_parameter::Mouse, nucleus::event_parameter::Wheel>;

public:
    explicit MyFrameBufferObject(QQuickItem *parent = 0);
    ~MyFrameBufferObject() override;
    Renderer *createRenderer() const Q_DECL_OVERRIDE;

signals:

    void frame_limit_changed();

    void mouse_pressed(const nucleus::event_parameter::Mouse&) const;
    void mouse_moved(const nucleus::event_parameter::Mouse&) const;
    void wheel_turned(const nucleus::event_parameter::Wheel&) const;
    void touch_made(const nucleus::event_parameter::Touch&) const;
    //    void viewport_changed(const glm::uvec2& new_viewport) const;
    void position_set_by_user(double new_latitude, double new_longitude);

    void virtual_resolution_factor_changed();

protected:
    void touchEvent(QTouchEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

public slots:
    void set_position(double latitude, double longitude);

private slots:
    void schedule_update();

public:
    [[nodiscard]] int frame_limit() const;
    void set_frame_limit(int new_frame_limit);

    [[nodiscard]] float virtual_resolution_factor() const;
    void set_virtual_resolution_factor(float new_virtual_resolution_factor);

private:
    int m_frame_limit = 60;
    float m_virtual_resolution_factor = 0.5f;
    QTimer* m_update_timer = nullptr;
    Q_PROPERTY(int frame_limit READ frame_limit WRITE set_frame_limit NOTIFY frame_limit_changed)
    Q_PROPERTY(float virtual_resolution_factor READ virtual_resolution_factor WRITE set_virtual_resolution_factor NOTIFY virtual_resolution_factor_changed)
};

#endif // MYFRAMEBUFFEROBJECT_H
