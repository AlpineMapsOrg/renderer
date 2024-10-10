/*****************************************************************************
 * AlpineMaps.org
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
#include <nucleus/tile_scheduler/setup.h>

namespace nucleus {
class DataQuerier;
}
namespace nucleus::camera {
class Controller;
}
namespace nucleus::maplabel {
class MapLabelFilter;
}
namespace nucleus::picker {
class PickerManager;
}
namespace nucleus::tile_scheduler::utils {
class AabbDecorator;
} // namespace nucleus::tile_scheduler::utils

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

    [[nodiscard]] std::shared_ptr<nucleus::tile_scheduler::utils::AabbDecorator> aabb_decorator() const;

    [[nodiscard]] std::shared_ptr<nucleus::DataQuerier> data_querier() const;

    [[nodiscard]] std::shared_ptr<nucleus::tile_scheduler::OldScheduler> scheduler() const;

    [[nodiscard]] std::shared_ptr<nucleus::picker::PickerManager> picker_manager() const;

    [[nodiscard]] std::shared_ptr<nucleus::maplabel::MapLabelFilter> label_filter() const;

signals:
    void initialised();

private:
    // WARNING: gl_engine::Context must be on the rendering thread!!
    mutable QMutex m_shared_ptr_mutex; // protects the shared_ptr
    std::shared_ptr<gl_engine::Context> m_engine_context;
    QThread* m_render_thread = nullptr;

    // the ones below are on the scheduler thread.
    nucleus::tile_scheduler::setup::MonolithicScheduler m_scheduler;
    std::shared_ptr<nucleus::DataQuerier> m_data_querier;
    std::unique_ptr<nucleus::camera::Controller> m_camera_controller;
    std::shared_ptr<nucleus::maplabel::MapLabelFilter> m_label_filter;
    std::shared_ptr<nucleus::picker::PickerManager> m_picker_manager;
    std::shared_ptr<nucleus::tile_scheduler::utils::AabbDecorator> m_aabb_decorator;
};
