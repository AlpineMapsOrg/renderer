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

#include "RenderThreadNotifier.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QRunnable>
#include <QThread>

RenderThreadNotifier::RenderThreadNotifier(QObject *parent)
    : QObject{parent}
{
    //    qDebug() << "RenderThreadNotifier::RenderThreadNotifier(QObject *parent) current thread: " << QThread::currentThread() << "  coreapp.thread: " << QCoreApplication::instance()->thread();
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        qDebug("RenderThreadNotifier must be created on the main thread!"); // so that it lives on the main thread event loop
        QCoreApplication::quit();
    }
}

RenderThreadNotifier* RenderThreadNotifier::instance()
{
    static RenderThreadNotifier instance;
    return &instance;
}

void RenderThreadNotifier::set_root_window(QQuickWindow* root_window)
{
    assert(m_root_window == nullptr);
    assert(root_window);
    m_root_window = root_window;
}

void RenderThreadNotifier::notify()
{
    // On webassembly, rendering happens in the main thread (even in the threaded version).
    // I don't understand why, but using the notification blocks the main thread after some time. Hence disableing it.
#ifndef __EMSCRIPTEN__
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
        qDebug() << "RenderThreadNotifier::notify() current thread: " << QThread::currentThread() << "  this.thread: " << this->thread();
    assert(QThread::currentThread() == QCoreApplication::instance()->thread());
    assert(m_root_window != nullptr);

    auto* runnable = QRunnable::create([]() {
//        qDebug() << "QCoreApplication::processEvents called on: " << QThread::currentThread() << "(" << QThread::currentThread()->objectName() << ")";
        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
    });
    m_root_window->scheduleRenderJob(runnable, QQuickWindow::RenderStage::NoStage);
#endif
}
