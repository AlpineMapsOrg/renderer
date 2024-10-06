/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

#include "RenderingContext.h"

#include "nucleus/utils/thread.h"
#include <QThread>

RenderingContext::RenderingContext(QObject* parent)
    : QObject { parent }
{
}

RenderingContext::~RenderingContext() { }

RenderingContext* RenderingContext::instance()
{

    static RenderingContext s_instance;
    return &s_instance;
}

void RenderingContext::initialise()
{
    QMutexLocker locker(&m_engine_context_mutex);
    if (m_engine_context)
        return;

    m_engine_context = std::make_shared<gl_engine::Context>();

    auto* render_thread = QThread::currentThread();
    connect(render_thread, &QThread::finished, m_engine_context.get(), &nucleus::EngineContext::destroy);

    nucleus::utils::thread::async_call(this, [this]() { this->initialised(); });
}

const std::shared_ptr<gl_engine::Context>& RenderingContext::engine_context() const
{
    QMutexLocker locker(&m_engine_context_mutex);
    return m_engine_context;
}
