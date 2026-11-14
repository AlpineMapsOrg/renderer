/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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

#include "ElevationProfile.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// steepness legend colours, same bins as the steepness overlay on the map
QColor steepness_color(double slope_degrees)
{
    if (slope_degrees < 15.0) return QColor(254, 249, 249);
    if (slope_degrees < 25.0) return QColor(0, 115, 25);
    if (slope_degrees < 30.0) return QColor(51, 249, 49);
    if (slope_degrees < 35.0) return QColor(242, 228, 44);
    if (slope_degrees < 40.0) return QColor(255, 169, 45);
    if (slope_degrees < 45.0) return QColor(255, 48, 45);
    if (slope_degrees < 50.0) return QColor(255, 79, 249);
    if (slope_degrees < 55.0) return QColor(183, 69, 253);
    if (slope_degrees < 60.0) return QColor(135, 44, 253);
    return QColor(49, 49, 253);
}

QRgb with_alpha(const QColor& colour, int alpha)
{
    return qRgba(colour.red(), colour.green(), colour.blue(), alpha);
}

} // namespace

ElevationProfile::ElevationProfile(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

void ElevationProfile::hoverMoveEvent(QHoverEvent* event)
{
    if (m_chart_rect.width() > 0) {
        m_cursor = std::clamp((event->position().x() - m_chart_rect.left()) / m_chart_rect.width(), 0.0, 1.0);
        update();
    }
}

void ElevationProfile::hoverLeaveEvent(QHoverEvent*)
{
    m_cursor = -1.0;
    update();
}

void ElevationProfile::mousePressEvent(QMouseEvent* event)
{
    if (m_chart_rect.width() > 0) {
        m_cursor = std::clamp((event->position().x() - m_chart_rect.left()) / m_chart_rect.width(), 0.0, 1.0);
        update();
    }
}

void ElevationProfile::set_profile(const QVariantList& points)
{
    if (m_profile == points) {
        return;
    }
    m_profile = points;
    emit profile_changed();
    update();
}

void ElevationProfile::set_progress(qreal progress)
{
    if (qFuzzyCompare(m_progress, progress)) {
        return;
    }
    m_progress = progress;
    emit progress_changed();
    update();
}

void ElevationProfile::paint(QPainter* painter)
{
    const QRectF bounds(0, 0, width(), height());
    painter->fillRect(bounds, QColor(0, 0, 0, 70));

    if (m_profile.size() < 2) {
        QFont font = painter->font();
        font.setPointSizeF(font.pointSizeF() * 0.9);
        painter->setFont(font);
        painter->setPen(QColor(255, 255, 255, 140));
        painter->drawText(bounds, Qt::AlignCenter, tr("no route"));
        return;
    }

    // margins: left for elevation labels, bottom for distance labels
    const qreal margin_left = 38;
    const qreal margin_bottom = 16;
    const qreal margin_top = 6;
    const qreal margin_right = 6;
    const QRectF chart(bounds.left() + margin_left, bounds.top() + margin_top,
        bounds.width() - margin_left - margin_right,
        bounds.height() - margin_top - margin_bottom);

    // bounds of the data
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    double max_x = 0.0;
    for (const QVariant& entry : m_profile) {
        const QVariantMap point = entry.toMap();
        min_y = std::min(min_y, point.value("y").toDouble());
        max_y = std::max(max_y, point.value("y").toDouble());
        max_x = std::max(max_x, point.value("x").toDouble());
    }
    // pad the elevation range so the line is not glued to the edges
    const double y_pad = std::max((max_y - min_y) * 0.08, 5.0);
    const double y_min = min_y - y_pad;
    const double y_max = max_y + y_pad;
    const double x_max = std::max(max_x, 1.0);
    const double y_range = std::max(y_max - y_min, 1.0);

    m_chart_rect = chart;
    const auto to_pixel = [&](double x, double y) {
        return QPointF(chart.left() + x / x_max * chart.width(),
            chart.bottom() - (y - y_min) / y_range * chart.height());
    };

    // axes + labels
    QFont font = painter->font();
    font.setPointSizeF(std::max(font.pointSizeF() * 0.72, 6.0));
    painter->setFont(font);
    painter->setPen(QColor(255, 255, 255, 200));
    painter->drawText(QRectF(bounds.left(), chart.top() - margin_top, margin_left - 4, 14),
        Qt::AlignRight | Qt::AlignTop, QString::number(int(max_y - y_pad)));
    painter->drawText(QRectF(bounds.left(), chart.bottom() - 14, margin_left - 4, 14),
        Qt::AlignRight | Qt::AlignBottom, QString::number(int(min_y + y_pad)));
    painter->drawText(QRectF(chart.left(), chart.bottom() + 1, chart.width(), 14),
        Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"));
    painter->drawText(QRectF(chart.left(), chart.bottom() + 1, chart.width(), 14),
        Qt::AlignRight | Qt::AlignVCenter, QString::number(x_max / 1000.0, 'f', 1) + QStringLiteral(" km"));
    painter->setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter->drawLine(QPointF(chart.left(), chart.top()), QPointF(chart.left(), chart.bottom()));
    painter->drawLine(QPointF(chart.left(), chart.bottom()), QPointF(chart.right(), chart.bottom()));

    // the profile: each segment stroked and filled with its slope colour.
    // the fill fades out towards the chart bottom so steep segments read as
    // more present than flat ones without hiding the segments behind them
    painter->setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i + 1 < m_profile.size(); ++i) {
        const QVariantMap a = m_profile.at(i).toMap();
        const QVariantMap b = m_profile.at(i + 1).toMap();
        const QPointF pa = to_pixel(a.value("x").toDouble(), a.value("y").toDouble());
        const QPointF pb = to_pixel(b.value("x").toDouble(), b.value("y").toDouble());
        if (std::abs(pb.x() - pa.x()) < 0.3) {
            continue; // thinner than a pixel, the next segment covers it
        }
        const QColor colour = steepness_color(b.value("slope").toDouble());

        QPolygonF fill_polygon;
        fill_polygon << pa << pb << QPointF(pb.x(), chart.bottom()) << QPointF(pa.x(), chart.bottom());
        QLinearGradient fill_gradient(pa.x(), pa.y(), pb.x(), chart.bottom());
        fill_gradient.setColorAt(0.0, with_alpha(colour, 100));
        fill_gradient.setColorAt(1.0, with_alpha(colour, 25));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill_gradient);
        painter->drawPolygon(fill_polygon);

        painter->setPen(QPen(colour, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(pa, pb);
    }

    // fly-through progress marker
    if (m_progress >= 0.0 && m_progress <= 1.0) {
        const qreal marker_x = chart.left() + m_progress * chart.width();
        painter->setPen(QPen(QColor(255, 255, 255, 220), 2.0));
        painter->drawLine(QPointF(marker_x, chart.top()), QPointF(marker_x, chart.bottom()));
    }

    // crosshair readout: steepness, distance and vertical from the start at
    // the hovered/tapped position
    if (m_cursor >= 0.0 && m_profile.size() >= 2) {
        const double cursor_x = m_cursor * x_max;
        const QVariantMap first = m_profile.first().toMap();
        const double y_start = first.value("y").toDouble();
        QVariantMap nearest = m_profile.first().toMap();
        double best_distance = std::numeric_limits<double>::max();
        for (const QVariant& entry : m_profile) {
            const QVariantMap point = entry.toMap();
            const double dx = std::abs(point.value("x").toDouble() - cursor_x);
            if (dx < best_distance) {
                best_distance = dx;
                nearest = point;
            }
        }
        const double distance = nearest.value("x").toDouble();
        const double slope = nearest.value("slope").toDouble();
        const double vertical = nearest.value("y").toDouble() - y_start;

        const QString distance_text = distance >= 1000.0
            ? QString::number(distance / 1000.0, 'f', 1) + QStringLiteral(" km")
            : QString::number(int(distance)) + QStringLiteral(" m");
        const QString vertical_text = (vertical >= 0.0 ? QStringLiteral("+") : QStringLiteral("-"))
            + QString::number(int(std::abs(vertical))) + QStringLiteral(" m");
        // degree sign via QChar: a literal in the source breaks the mingw
        // execution charset conversion
        const QString info = distance_text + QStringLiteral("  |  ")
            + QString::number(int(slope)) + QChar(0xB0) + QStringLiteral("  |  ") + vertical_text;

        const qreal cx = chart.left() + m_cursor * chart.width();
        painter->setPen(QPen(QColor(255, 255, 255, 180), 1.0));
        painter->drawLine(QPointF(cx, chart.top()), QPointF(cx, chart.bottom()));

        const QFontMetrics metrics(font);
        const QRectF text_rect = metrics.boundingRect(info).adjusted(-6, -3, 6, 3);
        qreal box_x = cx - text_rect.width() / 2.0;
        box_x = std::clamp(box_x, bounds.left() + 2.0, bounds.right() - text_rect.width() - 2.0);
        const QRectF box(box_x, bounds.top() + 2.0, text_rect.width(), text_rect.height());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 190));
        painter->drawRoundedRect(box, 3.0, 3.0);
        painter->setPen(QColor(255, 255, 255, 235));
        painter->drawText(box, Qt::AlignCenter, info);
    }
}
