/*****************************************************************************
 * Alpine Terrain Renderer
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

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QColor>
#include <QVector3D>

class TimerFrontendObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ get_name CONSTANT)
    Q_PROPERTY(QString group READ get_group CONSTANT)
    Q_PROPERTY(float last_measurement READ get_last_measurement CONSTANT)
    Q_PROPERTY(float average READ get_average CONSTANT)
    Q_PROPERTY(float quick_average READ get_quick_average CONSTANT)
    Q_PROPERTY(QColor color READ get_color CONSTANT)
    Q_PROPERTY(QList<QVector3D> measurements MEMBER m_measurements)

public:

    static constexpr const QColor timer_colors[] = {
        QColor(255, 0, 0),      // red
        QColor(0, 255, 255),    // cyan
        QColor(125, 0, 255),    // violet
        QColor(125, 255, 0),    // spring green
        QColor(255, 0, 255),    // magenta
        QColor(0, 125, 255),    // ocean
        QColor(0, 255, 0),      // green
        QColor(255, 125, 0),    // orange
        QColor(0, 0, 255),      // blue
        QColor(0, 255, 125),    // turquoise
        QColor(255, 255, 0),    // yellow
        QColor(255, 0, 125)     // raspberry
    };

    static int timer_color_index;

    void add_measurement(float value, int frame);
    float get_last_measurement();
    float get_average();
    float get_quick_average() { return m_quick_average; }


    QString get_name() { return m_name; }
    QString get_group() { return m_group; }
    QColor get_color() { return m_color;    }

    TimerFrontendObject(QObject* parent, const QString& name, const QString& group, const int queue_size = 30, const float average_weight = 1.0/30.0f, const float first_value = 0.0 );
    ~TimerFrontendObject();

    bool operator!=(const TimerFrontendObject&) const
    {
        // ToDo compare for difference
        return true;
    }

private:
    QString m_name;
    QString m_group;
    QList<QVector3D> m_measurements;
    QColor m_color;
    float m_new_weight = 1.0/30.0;
    float m_old_weight = 29.0/30.0;
    float m_quick_average = 0.0f;
    int m_queue_size = 30;

};
