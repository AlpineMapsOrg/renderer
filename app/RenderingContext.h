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

#include <QQmlEngine>

// move to pimpl to avoid including all the stuff in the header.

namespace gl_engine {
class Context;
}
namespace nucleus {
class DataQuerier;
}
namespace nucleus::map_label {
class Filter;
}
namespace nucleus::map_label {
class Scheduler;
}
namespace nucleus::picker {
class PickerManager;
}
namespace nucleus::tile {
class GeometryScheduler;
class TextureScheduler;
class SchedulerDirector;
}
namespace nucleus::tile::utils {
class AabbDecorator;
}

class RenderingContext : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    explicit RenderingContext(QObject* parent = nullptr);
    struct Data;
    std::unique_ptr<Data> m;

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
    void destroy();
    [[nodiscard]] std::shared_ptr<gl_engine::Context> engine_context() const;
    [[nodiscard]] std::shared_ptr<nucleus::tile::utils::AabbDecorator> aabb_decorator() const;
    [[nodiscard]] std::shared_ptr<nucleus::DataQuerier> data_querier() const;
    [[nodiscard]] nucleus::tile::GeometryScheduler* geometry_scheduler() const;
    [[nodiscard]] std::shared_ptr<nucleus::picker::PickerManager> picker_manager() const;
    [[nodiscard]] std::shared_ptr<nucleus::map_label::Filter> label_filter() const;
    [[nodiscard]] nucleus::map_label::Scheduler* map_label_scheduler() const;
    [[nodiscard]] nucleus::tile::TextureScheduler* ortho_scheduler() const;
    [[nodiscard]] nucleus::tile::SchedulerDirector* scheduler_director() const;

signals:
    void initialised();
};
