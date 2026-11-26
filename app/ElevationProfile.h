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

#pragma once

#include <QQuickPaintedItem>
#include <QVariantList>

// elevation profile chart of the planned route. the line is coloured by the
// slope angle of each segment (same legend colours as the map), and the area
// below is filled with the same colour at low opacity. a progress marker can
// be shown (fly-through preview).
class ElevationProfile : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    // entries: { x: metres along the route, y: elevation (m), slope: degrees }
    Q_PROPERTY(QVariantList profile READ profile WRITE set_profile NOTIFY profile_changed)
    // 0..1 marker position along the route, < 0 hides the marker
    Q_PROPERTY(qreal progress READ progress WRITE set_progress NOTIFY progress_changed)

public:
    explicit ElevationProfile(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    [[nodiscard]] const QVariantList& profile() const { return m_profile; }
    void set_profile(const QVariantList& points);
    [[nodiscard]] qreal progress() const { return m_progress; }
    void set_progress(qreal progress);

protected:
    // crosshair readout: hover (desktop) and tap (touch)
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void profile_changed();
    void progress_changed();

private:
    QVariantList m_profile;
    qreal m_progress = -1.0;
    qreal m_cursor = -1.0; // 0..1 along the route, -1 = hidden
    QRectF m_chart_rect; // updated during paint, used by the input events
};
