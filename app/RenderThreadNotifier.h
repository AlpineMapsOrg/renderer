/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

class QQuickWindow;

/**
 * @brief The purpose of this singleton is to wake the render thread up, so it can process signals and slots.
 *
 * Why is it necessary?
 *      In Qt 6.4, the render thread goes to sleep after rendering is done. It is then unavailable for signals.
 *      However, alpine renderer relies on signals and slots being delivered to the rendering thread, e.g., for
 *      delivery of tiles (will be loaded onto the GPU), and for processing interaction events (which needs GPU
 *      processing to do a ray cast for getting the rotation point).
 *
 * How to use it?
 *      QQuickWindow::scheduleRenderJob is used to deliver a NoStage job, which calls QCoreApplication::processEvents.
 *      That one is not thread safe (at least not according to the docs), so it needs to be called from the main (GUI)
 *      thread.
 */
class RenderThreadNotifier : public QObject {
    Q_OBJECT

    QQuickWindow* m_root_window = nullptr;
    explicit RenderThreadNotifier(QObject* parent = nullptr);
    ~RenderThreadNotifier() override = default;

public:
    RenderThreadNotifier(const RenderThreadNotifier& other) = delete;
    void operator=(const RenderThreadNotifier& other) = delete;

    static RenderThreadNotifier* instance();

    // should be called only once
    void set_root_window(QQuickWindow* root_window);

public slots:
    /// this one is not thread safe and should be called only on the main thread (e.g., via a signals and slots connection)
    void notify();

signals:
    void redraw_requested(); // all render items should connect this to their redraw timers. all others can trigger this to request redraws.
};
