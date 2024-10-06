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

#pragma once

#include <QMutex>
#include <QQmlEngine>
#include <gl_engine/Context.h>

class RenderingContext : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    explicit RenderingContext(QObject* parent = nullptr);

public:
    RenderingContext(RenderingContext const&) = delete;
    ~RenderingContext() override;
    void operator=(RenderingContext const&) = delete;

    static RenderingContext* create(QQmlEngine*, QJSEngine*)
    {
        QJSEngine::setObjectOwnership(instance(), QJSEngine::CppOwnership);
        return instance();
    }
    static RenderingContext* instance();

    void initialise();
    [[nodiscard]] const std::shared_ptr<gl_engine::Context>& engine_context() const;

signals:
    void initialised();

private:
    // WARNING: gl_engine::Context must be on the rendering thread!!
    mutable QMutex m_engine_context_mutex; // protects the shared_ptr
    std::shared_ptr<gl_engine::Context> m_engine_context;
};
